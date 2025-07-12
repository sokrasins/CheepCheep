/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

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

#include "esp_app_desc.h"

const config_t *config;

static status_t server_cmd_handler(msg_t *msg);

static int _reboot(int argc, char **argv);
static int _flash(int argc, char **argv);

void task_info(void);

void f_startMemory(void);
void f_PrintTasks(void *parameters);
void f_HeapMonitor(void *pvParameter);
void f_PrintHeapHistorico(uint32_t atual);
void f_stopMemory(void *arg);
void print_mem_chart(uint32_t mem_atual);

void app_main(void)
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

    console_register("reboot", "reboot the device", NULL, _reboot);
    console_register("flash", "enter bootloader to flash device", NULL, _flash);

    status = nvstate_init();
    if (status != STATUS_OK) { ERROR("nvstate_init failed: %ld", status); }

    INFO("Getting config");
    config = config_get();
    if (config == NULL)
    { 
        ERROR("Couldn't get device config. Can't proceed without a config."); 
        while(1);
    }

    INFO("Starting console");
    console_start();

    INFO("Setting up gpio");
    status = gpio_init(&config->pins, &config->general);
    if (status != STATUS_OK) { ERROR("gpio_init failed: %ld", status); }

    if (config->general.wiegand_enabled)
    {
        INFO("Setting up reader");
        status = wieg_init(
            config->pins.wiegand_zero, 
            config->pins.wiegand_one, 
            config->general.uid_32bit_mode ? WIEG_32_BIT : WIEG_24_BIT
        );
        if (status != STATUS_OK) { ERROR("wieg_init failed: %ld", status); }
    }
    else
    {
        ERROR("Configuration Error: Wiegand must be enabled");
        // TODO: Add rdm6300 lib
    }

    INFO("Setting up client");
    status = client_init(&config->client, config->device_type);
    client_handler_register(server_cmd_handler);
    client_open();

    INFO("Setting up authorized tag db");
    status = tags_init();
    if (status != STATUS_OK) { ERROR("tags_init failed: %ld", status); }

    switch(config->device_type) {
        case DEVICE_DOOR:
            INFO("Initializing door");
            status = door_init(config);
            break;

        case DEVICE_INTERLOCK:
            INFO("Initializing interlock");
            status = interlock_init(config);
            break;

        case DEVICE_VENDING:
            INFO("Initializing vending");
            status = vending_init(config);
            break;

        default:
            ERROR("Invalid device specified: %d.\nCheck configuration and reflash.", config->device_type);
    }
    if (status != STATUS_OK) { ERROR("device init failed: %lu", status); }

    f_startMemory();

    while(1)
    {
        //task_info();
        vTaskDelay(pdMS_TO_TICKS(60000));
    }
}

static status_t server_cmd_handler(msg_t *msg)
{
    status_t status = -STATUS_INVALID;
    if (msg->type == MSG_REBOOT)
    {
        esp_restart();
        status = STATUS_OK;
    }

    return status;
}

static int _reboot(int argc, char **argv)
{
    sys_restart();
    return 0;
}

static int _flash(int argc, char **argv)
{
    sys_enter_boot();
    return 0;
}

TaskHandle_t xHandle;
TaskStatus_t xTaskDetails;
void print_stack_info(char *name)
{
    xHandle = xTaskGetHandle(name);
    if (xHandle)
    {
        vTaskGetInfo(
            xHandle,
            &xTaskDetails,
            pdTRUE,
            eInvalid 
        );  

        INFO("Task:           %s", name);
        INFO("    state:      %u", xTaskDetails.eCurrentState);
        INFO("    runtime:    %u", xTaskDetails.ulRunTimeCounter);
        INFO("    free stack: %u", xTaskDetails.usStackHighWaterMark);
    }
    else
    {
        INFO("No handle for task %s", name);
    }
}

void task_info(void)
{
    size_t heap_size = xPortGetMinimumEverFreeHeapSize();
    INFO("Current heap: %u", heap_size);
    print_stack_info("Door_Task");
    print_stack_info("Wiegand_Task");
    print_stack_info("Signal_Task");
    print_stack_info("Net_Task");
    print_stack_info("DFU_Task");
}


#define CONFIG_HEAP_TASK_TRACKING 1
#include "esp_heap_trace.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "esp_log.h"

#define TAG "Memory:"
#define HIST_SIZE 20
#define NUM_RECORDS 100
static heap_trace_record_t trace_record[NUM_RECORDS];
static uint32_t heap_history[HIST_SIZE] = {0};
static uint8_t heap_index = 0;
static bool heap_history_full = false;
static bool StopMemory = false;

SemaphoreHandle_t SemaphorePrintMem;
TaskHandle_t hPrintMem = NULL;
TaskHandle_t hHeapMonitor = NULL;

void f_startMemory(void){
    if(hPrintMem == NULL && hHeapMonitor == NULL){
        xTaskCreate(f_PrintTasks, "f_PrintTasks", 8196, NULL, tskIDLE_PRIORITY+5, &hPrintMem);
        xTaskCreate(f_HeapMonitor, "f_HeapMonitor", 4096, NULL, tskIDLE_PRIORITY+5, &hHeapMonitor);
    }
}

void f_stopMemory(void *arg) {
      StopMemory = true;
      vTaskPrioritySet(hPrintMem, 0);
      vTaskPrioritySet(hHeapMonitor, 0);
      if(SemaphorePrintMem){xSemaphoreTake(SemaphorePrintMem, portMAX_DELAY);}
      vTaskDelete(NULL);
}

void f_startStopMemory(void){
    xTaskCreate(f_stopMemory, "f_stopMemory", 2500, NULL, tskIDLE_PRIORITY+10, NULL);
}

void setupMemory(void){
        SemaphorePrintMem = xSemaphoreCreateBinary();
}

void f_PrintTasks(void *parameters) {
            StopMemory= false;
            if (SemaphorePrintMem==NULL){
                SemaphorePrintMem = xSemaphoreCreateBinary();
            }
            xSemaphoreGive(SemaphorePrintMem);
            while (!StopMemory) {
                    xSemaphoreTake(SemaphorePrintMem, portMAX_DELAY);
                    uint32_t min_heap_atual = esp_get_free_heap_size();
                    uint32_t min_heap = esp_get_minimum_free_heap_size();
                    UBaseType_t numTasks;
                    TaskStatus_t *taskStatusArray;
                    numTasks = uxTaskGetNumberOfTasks();
                    taskStatusArray = (TaskStatus_t *)pvPortMalloc(numTasks * sizeof(TaskStatus_t));
                    if (taskStatusArray != NULL) {
                            numTasks = uxTaskGetSystemState(taskStatusArray, numTasks, NULL);
                            printf("--------------------------------------------------------------------------------------------\n");
                            printf("| %-17s | %-9s | %-8s | %-20s | %-6s |\n", "Task Name", "Status", "Priority", "Stack High Water Mark", "Core");
                            printf("--------------------------------------------------------------------------------------------\n");
                            for (int i = 0; i < numTasks; i++) {
                                    const char *core;
                                    core = "0";

                                    printf("| %-17s | %-9s | %-8u | %-20lu | %-6s |\n",
                                        taskStatusArray[i].pcTaskName,
                                        (taskStatusArray[i].eCurrentState == eRunning ? "Running" : "Blocked"),
                                        taskStatusArray[i].uxCurrentPriority,
                                        taskStatusArray[i].usStackHighWaterMark,
                                        core);
                            }
                            printf("--------------------------------------------------------------------------------------------\n");
                            printf("Current memory: %u, Lowest value: %u\n", (unsigned int)min_heap_atual, (unsigned int)min_heap);
                            printf("--------------------------------------------------------------------------------------------\n");
                            printf("Heap DRAM total: %d bytes\n", heap_caps_get_total_size(MALLOC_CAP_8BIT));
                            printf("Heap IRAM total: %d bytes\n", heap_caps_get_total_size(MALLOC_CAP_32BIT));
                            printf("--------------------------------------------------------------------------------------------\n");
                            f_PrintHeapHistorico(min_heap_atual);
                            //print_mem_chart(min_heap_atual);
                            // Free allocated memory
                            vPortFree(taskStatusArray);
                    }
                xSemaphoreGive(SemaphorePrintMem);
                vTaskDelay(pdMS_TO_TICKS(3000));
            }
            hPrintMem = NULL;
            vTaskDelete(NULL); // Kill this task
}

void f_memoria(char *tarefa){
        INFO("Task memory usage (%s): %d bytes", tarefa, uxTaskGetStackHighWaterMark(NULL) * sizeof(StackType_t));
}

void f_PrintHeapHistorico(uint32_t atual) {
        heap_history[heap_index] = atual;
        heap_index = (heap_index + 1) % HIST_SIZE;
        if (heap_index == 0) heap_history_full = true;

        uint32_t min = heap_history[0];
        uint32_t max = heap_history[0];
        uint8_t limite = heap_history_full ? HIST_SIZE : heap_index;

        for (int i = 1; i < limite; i++) {
            if (heap_history[i] < min) min = heap_history[i];
            if (heap_history[i] > max) max = heap_history[i];
        }

        printf("Current memory: %lu, Min last 60s: %lu, Max last 60s: %lu\n", atual, min, max);
}

void print_mem_chart(uint32_t mem_atual) {
        // Adjust there values according to your memory range
        const uint32_t mem_min = 169000;
        const uint32_t mem_max = 230000;
        const int largura_barra = 50;

        // NOrmalize the memory value to the bar value
        int barra = (int)(((float)(mem_atual - mem_min) / (mem_max - mem_min)) * largura_barra);
        if (barra < 0) barra = 0;
        if (barra > largura_barra) barra = largura_barra;

        // Print visual
        printf("Mem: %6lu |", mem_atual);
        for (int i = 0; i < barra; i++) printf("#");
        for (int i = barra; i < largura_barra; i++) printf(" ");
        printf("|\n");
}

void f_HeapMonitor(void *pvParameter) {
        vTaskDelay(pdMS_TO_TICKS(60000)); // wait 1 minute before starting heap trace

        INFO("Starting heap trace...");
        heap_trace_init_standalone(trace_record, NUM_RECORDS);
        heap_trace_start(HEAP_TRACE_LEAKS);

        vTaskDelay(pdMS_TO_TICKS(600000)); // Accumulate data for 10 minutes
        vTaskDelay(pdMS_TO_TICKS(600000)); // Accumulate data for 10 minutes

        heap_trace_stop();
        INFO("Ending tracking. Leaks detected:");

        int count = heap_trace_get_count();

        for (int i = 0; i < count; i++) {
            const heap_trace_record_t *rec = &trace_record[i];
            if (rec->address == NULL || rec->size == 0) continue;

            WARN("Allocation #%d: %u bytes in %p", i, rec->size, rec->address);

            for (int j = 0; j < CONFIG_HEAP_TRACING_STACK_DEPTH; j++) {
                if (rec->alloced_by[j]) {
                    const void *addr = rec->alloced_by[j];
                    //esp_rom_printf("    ↳ [%d] %p (%s)\n", j, addr, esp_backtrace_symbol(addr));
                    esp_rom_printf("    ↳ [%d] %p\n", j, rec->alloced_by[j]);

                }
            }

            vTaskDelay(pdMS_TO_TICKS(10)); // Make room for a watchdog
        }

        vTaskDelete(NULL);
}