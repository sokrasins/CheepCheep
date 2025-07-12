#include "wiegand.h"
#include "wiegand_fmt.h"
#include "log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"

#include <stddef.h>
#include <string.h>
#include <assert.h>

// Max number of event handlers allowed
#define WIEG_MAX_HANDLERS   10U

// If the parsing logic is waiting longer than this time, the card data is 
// considered an error and thrown out
#define WIEG_TIMEOUT        20U //ms

// Task config
#define WIEGAND_TASK_NAME       "Wiegand_Task" 
#define WIEGAND_TASK_STACK_SIZE 4096U // TODO: Find out why this is so high (probably some door task thing)
#define WIEGAND_TASK_PRIO       2U
static StackType_t wiegand_stack[WIEGAND_TASK_STACK_SIZE];
static StaticTask_t wiegand_task_buf;

typedef struct {
    int num_swipes;     // Number of ttoal swipes. This counts swipes with bad parity, but not timeouts.
    int num_bad_parity; // Number of swipes with a bad parity calculation
    int num_timeout;    // Number of times the parsing logic times out waiting for another bit
} wiegand_stats_t;

typedef enum {
    PARITY_EVEN,        // Total number of set bits is even (including parity)      
    PARITY_ODD,         // Total number of set bits is odd (including parity)
} parity_t;

typedef struct {
    void *ctx;
    wieg_evt_t event;
    wieg_evt_cb_t cb;
} handlers_t;

typedef struct {
    handlers_t handlers[WIEG_MAX_HANDLERS];
    const wieg_fmt_desc_t *fmt;
    QueueHandle_t pin_q;
    wiegand_stats_t stats;
} wieg_ctx_t;

static wieg_ctx_t _ctx;

static const int bit_0 = 0;
static const int bit_1 = 1;

// Helpers
void wieg_task(void *params);

static bool wieg_is_parity_good(const wieg_fmt_desc_t *fmt, uint32_t bits);
static void bits_to_card(const wieg_fmt_desc_t *fmt, uint32_t bits, card_t *card);
static bool parity(parity_t parity, uint32_t num);
static void gpio_interrupt_handler(void *args);

status_t wieg_init(int d0, int d1, wieg_encoding_t encode)
{
    esp_err_t err;

    // TODO: Implement 32-bit mode
    if (encode == WIEG_32_BIT)
    {
        ERROR("wiegand 32-bit mode not supported");
        return -STATUS_UNIMPL;
    }

    _ctx.fmt = encode == WIEG_24_BIT ? &wieg_fmt_24bit : &wieg_fmt_32bit;

    for (int i=0; i<WIEG_MAX_HANDLERS; i++)
    {
        _ctx.handlers[i].cb = NULL;
    }

    memset(&_ctx.stats, 0, sizeof(wiegand_stats_t));

    // TODO: let bsp manage d0 and d1 config
    // Set up gpio. Wiegand signals begin with a negative edge, so detect those 
    // for new bits
    err = gpio_set_direction(d0, GPIO_MODE_INPUT);
    err |= gpio_set_pull_mode(d0, GPIO_FLOATING);
    err |= gpio_set_intr_type(d0, GPIO_INTR_NEGEDGE);
    if (err != ESP_OK) { ERROR("Invalid settings for wiegand pin d0 (pin %u)", d0); return -STATUS_INVAL; }

    gpio_set_direction(d1, GPIO_MODE_INPUT);
    gpio_set_pull_mode(d1, GPIO_FLOATING);
    gpio_set_intr_type(d1, GPIO_INTR_NEGEDGE);
    if (err != ESP_OK) { ERROR("Invalid settings for wiegand pin d1 (pin %u)", d1); return -STATUS_INVAL; }

    // Set up the pin ISRs. The ctx provided defines the bit that each ISR adds 
    // to the card data.
    err = gpio_install_isr_service(0); // No flags, check the ISR priority
    if (err != ESP_OK) { ERROR("Error installing isr service: %s", esp_err_to_name(err)); return -STATUS_NO_RESOURCE; }
    err = gpio_isr_handler_add(d0, gpio_interrupt_handler, (void *)&bit_0);
    if (err != ESP_OK) { ERROR("Couldn't add d0 isr handler: %s", esp_err_to_name(err)); return -STATUS_NO_RESOURCE; }
    err = gpio_isr_handler_add(d1, gpio_interrupt_handler, (void *)&bit_1);
    if (err != ESP_OK) { ERROR("Couldn't add d1 isr handler: %s", esp_err_to_name(err)); return -STATUS_NO_RESOURCE; }

    // Set up queue for new bits and start task
    _ctx.pin_q = xQueueCreate(_ctx.fmt->total_bits, sizeof(int));
    if (_ctx.pin_q == NULL) { ERROR("Couldn't create the pin queue"); return -STATUS_NOMEM; }
    
    // Make task
    xTaskCreateStatic(
        wieg_task, 
        WIEGAND_TASK_NAME, 
        WIEGAND_TASK_STACK_SIZE, 
        (void *)&_ctx, 
        WIEGAND_TASK_PRIO, 
        wiegand_stack,
        &wiegand_task_buf
    );

    return STATUS_OK;
}

wieg_evt_handle_t wieg_evt_handler_reg(wieg_evt_t event, wieg_evt_cb_t cb, void *ctx)
{
    assert(cb);

    if (event == WIEG_EVT_NEWBIT)
    {
        return NULL;
    }

    for (int i=0; i<WIEG_MAX_HANDLERS; i++)
    {
        if (_ctx.handlers[i].cb == NULL)
        {
            _ctx.handlers[i].event = event;
            _ctx.handlers[i].cb = cb;
            _ctx.handlers[i].ctx = ctx;
            return (void *) &_ctx.handlers[i];
        }
    }
    return NULL;
}

status_t wieg_evt_handler_dereg(wieg_evt_handle_t handle)
{
    assert(handle);

    handlers_t *handler = (handlers_t *) handle;
    handler->cb = NULL;
    return STATUS_OK;
}

// Private

void wieg_task(void *params)
{
    assert(params);

    wieg_ctx_t *ctx = (wieg_ctx_t *) params;
    int ptr = ctx->fmt->total_bits - 1;
    uint32_t bits = 0;
    int bit = 0;
    card_t card;

    while (1)
    {
        bit = 0;
        if (xQueueReceive(ctx->pin_q, &bit, pdMS_TO_TICKS(WIEG_TIMEOUT)))
        {
            // Record new bit to data
            bits |= (bit << ptr);

            // Check if we have complete card data yet
            if (ptr == 0)
            {
                // Track swipes
                ctx->stats.num_swipes++;

                // Verify card data
                if (wieg_is_parity_good(ctx->fmt, bits))
                {
                    // Card data is valid, format bits into readable card data
                    bits_to_card(ctx->fmt, bits, &card);

                    // Fire NEWCARD events
                    for (int i=0; i<WIEG_MAX_HANDLERS; i++)
                    {
                        if(ctx->handlers[i].cb != NULL && ctx->handlers[i].event == WIEG_EVT_NEWCARD)
                        {
                            ctx->handlers[i].cb(
                                WIEG_EVT_NEWCARD,
                                &card,
                                ctx->handlers[i].ctx
                            );
                        }
                    }
                }
                else
                {
                    // Parity check failed, report bad scan
                    ctx->stats.num_bad_parity++;
                    ERROR("New swipe fails parity check: %lu", bits);
                }

                // Clear data to prepare for new card
                bits = 0;
                ptr = ctx->fmt->total_bits;
            }
            
            ptr--;
        }
        else
        {
            // Timeout before we got all the bits. Clear and start over
            bits = 0;
            ptr = ctx->fmt->total_bits - 1;
        }
    }
}

static void bits_to_card(const wieg_fmt_desc_t *fmt, uint32_t bits, card_t *card)
{
    assert(fmt);
    assert(card);

    card->raw = 0;
    card->user_id = (uint16_t) ((bits & fmt->uid_mask) >> fmt->uid_offset);
    card->facility = (uint16_t) ((bits & fmt->fac_mask) >> fmt->fac_offset);
}

static bool wieg_is_parity_good(const wieg_fmt_desc_t *fmt, uint32_t bits)
{
    assert(fmt);

    // Calc high parity (which is always even)
    uint32_t high = bits & fmt->high_mask;
    bool high_parity = parity(PARITY_EVEN, high);
    if (WIEG_HIGH_PARITY_BIT(fmt, bits) != high_parity) { return false; }

    // calc low parity (which is always odd)
    uint32_t low = bits & fmt->low_mask;
    bool low_parity = parity(PARITY_ODD, low);
    if (WIEG_LOW_PARITY_BIT(fmt, bits) != low_parity) { return false; }

    return true;
}

static bool parity(parity_t parity, uint32_t num)
{
    // By setting the initial value, the parity calculation can be either:
    // - even: initialize with 0
    // - odd: initialize with 1
    bool p = (bool) parity;
    for (int i=0; i<32; i++)
    {
        p ^= (num >> i) & 1;
    }
    return p;
}

// IRAM keeps this ISR clear from flash, which lets this ISR fire when flash 
// reads/writes happen
static void IRAM_ATTR gpio_interrupt_handler(void *args)
{
    assert(args);

    // The context tells us whether the bit was triggered from d0 or d1, and 
    // this which bit to add. Since the bit is encpded in args, pass it through 
    // the queue to the parsing task.
    BaseType_t wake_high_prio = pdFALSE;
    xQueueSendFromISR(_ctx.pin_q, args, &wake_high_prio);
}
