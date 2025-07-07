#include "signal.h"
#include "gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <assert.h>

// Event flags
#define SIGNAL_OK_FLAG          (1<<0)
#define SIGNAL_ALERT_FLAG       (1<<1)
#define SIGNAL_CARDREAD_FLAG    (1<<2)
#define SIGNAL_ACTION_FLAG      (1<<3)

// Signal timing
#define SIGNAL_OK_TIME          1000U //ms
#define SIGNAL_ALERT_TIME       300U //ms
#define SIGNAL_CARDREAD_TIME    200U //ms

#define SIGNAL_TASK_NAME "Signal_Task"
#define SIGNAL_TASK_STACK   2048U
#define SIGNAL_TASK_PRIO    2U

void signal_task(void *params);

static TaskHandle_t _signal_task_handle = NULL;
const config_buzzer_t *_config;

status_t signal_init(const config_buzzer_t *config)
{
    assert(config);
    _config = config;

    // Most of these signals involve seconds of waiting. We segment these into their own task.
    xTaskCreate(
        signal_task, 
        SIGNAL_TASK_NAME, 
        SIGNAL_TASK_STACK, 
        NULL, 
        SIGNAL_TASK_PRIO, 
        &_signal_task_handle
    );

    return STATUS_OK;
}

void signal_alert(void)
{
    if (_config->enabled)
    {
        xTaskNotify(_signal_task_handle, SIGNAL_ALERT_FLAG, eSetBits);
    }
}

void signal_ok(void)
{
    if (_config->enabled)
    {
        xTaskNotify(_signal_task_handle, SIGNAL_OK_FLAG, eSetBits);
    }
}

void signal_cardread(void)
{
    if (_config->enabled && _config->buzz_on_swipe)
    {
        xTaskNotify(_signal_task_handle, SIGNAL_CARDREAD_FLAG, eSetBits);
    }
}

void signal_action(void)
{
    if (_config->enabled)
    {
        xTaskNotify(_signal_task_handle, SIGNAL_ACTION_FLAG, eSetBits);
    }
}

void signal_task(void *params)
{
    while(1)
    {
        uint32_t flags = 0;
        if(xTaskNotifyWait(0x00, 0xFFFFFFFF, &flags, portMAX_DELAY) == pdPASS)
        {
            if(flags & SIGNAL_OK_FLAG)
            {
                gpio_out_set(OUTPUT_READER_BUZZER, true);
                vTaskDelay(pdMS_TO_TICKS(SIGNAL_OK_TIME));

                gpio_out_set(OUTPUT_READER_BUZZER, false);
            }
            if(flags & SIGNAL_ALERT_FLAG)
            {
                gpio_out_set(OUTPUT_READER_BUZZER, true);
                gpio_out_set(OUTPUT_READER_LED, true);
                vTaskDelay(pdMS_TO_TICKS(SIGNAL_ALERT_TIME));

                gpio_out_set(OUTPUT_READER_BUZZER, false);
                gpio_out_set(OUTPUT_READER_LED, false);
                vTaskDelay(pdMS_TO_TICKS(SIGNAL_ALERT_TIME));

                gpio_out_set(OUTPUT_READER_BUZZER, true);
                gpio_out_set(OUTPUT_READER_LED, true);
                vTaskDelay(pdMS_TO_TICKS(SIGNAL_ALERT_TIME));

                gpio_out_set(OUTPUT_READER_BUZZER, false);
                gpio_out_set(OUTPUT_READER_LED, false);
            }
            if (flags & SIGNAL_CARDREAD_FLAG)
            {
                gpio_out_set(OUTPUT_READER_BUZZER, true);
                vTaskDelay(pdMS_TO_TICKS(SIGNAL_CARDREAD_TIME));
                gpio_out_set(OUTPUT_READER_BUZZER, false);
            }
            if (flags & SIGNAL_ACTION_FLAG)
            {
                gpio_out_set(OUTPUT_READER_BUZZER, true);
                vTaskDelay(pdMS_TO_TICKS(_config->action_delay * 1000));
                gpio_out_set(OUTPUT_READER_BUZZER, false);   
            }
        }
    }
}
