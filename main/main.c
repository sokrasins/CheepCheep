/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "bsp.h"
#include "wiegand.h"
#include "log.h"
#include "config.h"
#include "nvstate.h"
#include "device.h"
#include "tags.h"
#include "client.h"
#include "ota_dfu.h"
#include "console.h"
#include "debug_mem.h"

#include "esp_app_desc.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "sdkconfig.h"

// Threshold of heap bytes before the system gets reset. Right now the
// application idles at around ~150 kB head free, so if the heap falls below 10
// kB we're gonna overrun the heap before long.
#define HEAP_RESET_THRESHOLD 10000U // bytes

// Global configuration. This is either populated by defaults or by values saves
// in non-volatile storage.
const config_t *config = NULL;

// Device type. This gets populated based on the device type key in the config.
const device_t *device = NULL;

static status_t server_cmd_handler(msg_t *msg);

static int _reboot(int argc, char **argv);
static int _flash(int argc, char **argv);

static void halt(void);

void
app_main (void)
{
    status_t status;

    const esp_app_desc_t *desc = esp_app_get_description();
    INFO("");
    INFO("*********************************");
    INFO("********** CHEEP CHEEP **********");
    INFO("**********   v%s    **********", desc->version);
    INFO("*********************************");
    INFO("Built: %s, %s", desc->date, desc->time);
    INFO("");

    // If the application is new, this will mark it as runnable. Otherwise, the
    // application may roll back to a previous version
    ota_mark_application(true);

    // Register some system-level command handlers
    console_register("reboot", "reboot the device", NULL, _reboot);
    console_register("flash", "enter bootloader to flash device", NULL, _flash);

    // Initialize non-volatile state
    status = nvstate_init();
    if (status != STATUS_OK)
    {
        ERROR("nvstate_init failed: %ld", status);
        halt();
    }

    // Load the device configuration
    INFO("Getting config");
    config = config_get();
    if (config == NULL)
    {
        ERROR("Couldn't get device config. Can't proceed without a config.");
        halt();
    }

    // Start the UART configuration console
    INFO("Starting console");
    console_start();

    // Initialize the gpio according to the config params
    INFO("Setting up gpio");
    status = gpio_init(&config->pins, &config->general);
    if (status != STATUS_OK)
    {
        ERROR("gpio_init failed: %ld", status);
        halt();
    }

    // Set up the reader specified by the config
    if (config->general.wiegand_enabled)
    {
        INFO("Setting up reader");
        status = wieg_init(config->pins.wiegand_zero,
                           config->pins.wiegand_one,
                           config->general.uid_32bit_mode ? WIEG_32_BIT
                                                          : WIEG_24_BIT);
        if (status != STATUS_OK)
        {
            ERROR("wieg_init failed: %ld", status);
            halt();
        }
    }
    else
    {
        ERROR("Configuration Error: Wiegand must be enabled");
        // TODO: Add rdm6300 lib
    }

    // Configure the client. After this, the board will maintan a connection to
    // the server (specified in the config).
    INFO("Setting up client");
    status = client_init(&config->client, config->device_type);
    client_handler_register(server_cmd_handler);
    client_open();

    // Load the existing card database from flash
    INFO("Setting up authorized tag db");
    status = tags_init();
    if (status != STATUS_OK)
    {
        ERROR("tags_init failed: %ld", status);
        halt();
    }

    // Get the correct device. The config specifies the device type, so we get
    // the pointer to the device object using that as a key.
    device = device_get(config->device_type);
    if (NULL == device)
    {
        ERROR("Invalid device specified: %d.\nCheck configuration and reflash.",
              config->device_type);
        halt();
    }

    // Initialize the device type
    INFO("Initializing device");
    status = device->init(config);
    if (status != STATUS_OK)
    {
        ERROR("device init failed: %lu", status);
        halt();
    }

#ifdef CONFIG_HEAP_TRACING
    // Start stack and heap trace
    debug_mem_start();
#endif /*CONFIG_HEAP_TRACING*/

    size_t heap_size = 0;
    while (1)
    {
        // The main thread doesn't do anything but watch dynamic memory

        // Wait a minute
        vTaskDelay(pdMS_TO_TICKS(60000));

        // Check to make sure our heap is okay
        heap_size = xPortGetMinimumEverFreeHeapSize();
        if (heap_size < HEAP_RESET_THRESHOLD)
        {
            ERROR(
                "Remaining heap (%u bytes) is below minimum threshold (%u "
                "bytes), resetting.",
                heap_size,
                HEAP_RESET_THRESHOLD);
            sys_restart();
        }
    }
}

static status_t
server_cmd_handler (msg_t *msg)
{
    status_t status = -STATUS_INVALID;
    if (msg->type == MSG_REBOOT)
    {
        if (device != NULL)
        {
            device->deinit();
        }

        sys_restart();

        // TODO: Unreachable
        status = STATUS_OK;
    }

    return status;
}

static int
_reboot (int argc, char **argv)
{
    if (device != NULL)
    {
        device->deinit();
    }

    sys_restart();
    return 0;
}

static int
_flash (int argc, char **argv)
{
    sys_enter_boot();
    return 0;
}

// TODO: Is halting desired? only for debug?
static void
halt (void)
{
    while (1)
        ;
}