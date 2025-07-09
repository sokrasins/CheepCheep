#include "gpio.h"
#include "log.h"

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define GPIO_NULL_PIN 0xFF

typedef struct {
    int pin;
    bool rev;
    bool init;
} pin_ctx_t;

pin_ctx_t _input_pins[INPUT_INVAL];
pin_ctx_t _output_pins[OUTPUT_INVAL];

static void _set_pin_ctx(pin_ctx_t *ctx, int pin, bool rev);

status_t gpio_init(const config_pins_t *pins, const config_general_t *gen)
{
    assert(pins);
    assert(gen);

    esp_err_t err;

    _set_pin_ctx(&_input_pins[INPUT_AUX1], pins->aux_1, false);
    _set_pin_ctx(&_input_pins[INPUT_AUX2], pins->aux_2, false);
    _set_pin_ctx(&_input_pins[INPUT_DOOR_SENSOR], pins->door_sensor, false);
    _set_pin_ctx(&_input_pins[INPUT_IN1], pins->in_1, false);

    _set_pin_ctx(&_output_pins[OUTPUT_STATUS_LED], pins->status_led, false);
    _set_pin_ctx(&_output_pins[OUTPUT_READER_LED], pins->reader_led, false);
    _set_pin_ctx(&_output_pins[OUTPUT_READER_BUZZER], pins->reader_buzzer, false);
    _set_pin_ctx(&_output_pins[OUTPUT_RELAY], pins->relay, gen->relay_reversed);
    _set_pin_ctx(&_output_pins[OUTPUT_LOCK], pins->lock, gen->lock_reversed);
    _set_pin_ctx(&_output_pins[OUTPUT_OUT1], pins->out_1, gen->out_1_reversed);

    for (int i=0; i<INPUT_INVAL; i++)
    {
        pin_ctx_t *ctx = &_input_pins[i];
        if (ctx->init)
        {
            err = gpio_set_direction(ctx->pin, GPIO_MODE_INPUT);
            if (err != ESP_OK)
            {
                ERROR("Can't set pin %u to input: %s", ctx->pin, esp_err_to_name(err));
            }
        }
    }

    for (int i=0; i<OUTPUT_INVAL; i++)
    {
        pin_ctx_t *ctx = &_output_pins[i];
        if (ctx->init)
        {
            err = gpio_set_direction(ctx->pin, GPIO_MODE_OUTPUT);
            if (err != ESP_OK)
            {
                ERROR("Can't set pin %u to output: %s", ctx->pin, esp_err_to_name(err));
            }
        }
    }

    err = gpio_set_pull_mode(_input_pins[INPUT_AUX1].pin, GPIO_PULLDOWN_ONLY);
    if (err != ESP_OK) { ERROR("Can't set aux 1 pulldown: pin %u", _input_pins[INPUT_AUX1].pin); }
    err = gpio_set_pull_mode(_input_pins[INPUT_AUX2].pin, GPIO_PULLDOWN_ONLY);
    if (err != ESP_OK) { ERROR("Can't set aux 2 pulldown: pin %u", _input_pins[INPUT_AUX2].pin); }

    // Debug pulse. This is used in long-term testing to detect board reboots.
    // TODO: Remove
    gpio_out_set(OUTPUT_OUT1, true);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_out_set(OUTPUT_OUT1, false);

    // Set default pin states
    // TODO: these should be owned by device/reader
    gpio_out_set(OUTPUT_READER_BUZZER, false);
    gpio_out_set(OUTPUT_READER_LED, false);
    gpio_out_set(OUTPUT_LOCK, false);
    gpio_out_set(OUTPUT_RELAY, false);
    
    return STATUS_OK;
}

status_t gpio_out_set(output_t out, bool state)
{
    if (out >= OUTPUT_INVAL)
    {
        return -STATUS_INVAL;
    }

    pin_ctx_t *ctx = &_output_pins[out];

    if (!ctx->init)
    {
        return -STATUS_UNAVAILABLE;
    }

    esp_err_t err = gpio_set_level(ctx->pin, (int) (state ^ ctx->rev));
    if (err != ESP_OK) 
    { 
        ERROR("Invalid pin: %u", ctx->pin);
        return -STATUS_INVAL;
    }

    return STATUS_OK;
}

int gpio_in_get(input_t in)
{
    if (in >= INPUT_INVAL)
    {
        return -STATUS_INVAL;
    }

    pin_ctx_t *ctx = &_input_pins[in];

    if (!ctx->init)
    {
        return -STATUS_UNAVAILABLE; 
    }

    return (int) (ctx->rev ^ ((bool)gpio_get_level(ctx->pin)));
}

// Helpers

static void _set_pin_ctx(pin_ctx_t *ctx, int pin, bool rev)
{
    assert(ctx);

    ctx->pin = pin;
    ctx->rev = rev;
    ctx->init = pin != GPIO_NULL_PIN;
}
