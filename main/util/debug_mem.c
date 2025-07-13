// Adapted from: https://forums.freertos.org/t/how-i-solved-the-memory-leak-problem-in-a-complex-program-with-many-tasks/23035

#include "debug_mem.h"
#include "log.h"

#include "sdkconfig.h"

#ifdef CONFIG_HEAP_TRACING

#include "esp_heap_trace.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

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

static void f_stopMemory(void *arg);
void f_startStopMemory(void);
void setupMemory(void);
void f_PrintTasks(void *parameters);
void f_PrintHeapHistorico(uint32_t atual);
void f_HeapMonitor(void *pvParameter);

void debug_mem_start(void)
{
    if(hPrintMem == NULL && hHeapMonitor == NULL)
    {
        xTaskCreate(f_PrintTasks, "f_PrintTasks", 8196, NULL, tskIDLE_PRIORITY+5, &hPrintMem);
        xTaskCreate(f_HeapMonitor, "f_HeapMonitor", 4096, NULL, tskIDLE_PRIORITY+5, &hHeapMonitor);
    }
}

static void f_stopMemory(void *arg) 
{
    StopMemory = true;
    vTaskPrioritySet(hPrintMem, 0);
    vTaskPrioritySet(hHeapMonitor, 0);
    if(SemaphorePrintMem)
    {
        xSemaphoreTake(SemaphorePrintMem, portMAX_DELAY);
    }
    vTaskDelete(NULL);
}

void f_startStopMemory(void)
{
    xTaskCreate(f_stopMemory, "f_stopMemory", 2500, NULL, tskIDLE_PRIORITY+10, NULL);
}

void setupMemory(void)
{
    SemaphorePrintMem = xSemaphoreCreateBinary();
}

void f_PrintTasks(void *parameters) 
{
    StopMemory= false;
    if (SemaphorePrintMem==NULL)
    {
        SemaphorePrintMem = xSemaphoreCreateBinary();
    }
    xSemaphoreGive(SemaphorePrintMem);
           
    while (!StopMemory) 
    {
        xSemaphoreTake(SemaphorePrintMem, portMAX_DELAY);
        uint32_t min_heap_atual = esp_get_free_heap_size();
        uint32_t min_heap = esp_get_minimum_free_heap_size();
        UBaseType_t numTasks;
        TaskStatus_t *taskStatusArray;
        numTasks = uxTaskGetNumberOfTasks();
        taskStatusArray = (TaskStatus_t *)pvPortMalloc(numTasks * sizeof(TaskStatus_t));
        if (taskStatusArray != NULL) 
        {
            numTasks = uxTaskGetSystemState(taskStatusArray, numTasks, NULL);
            printf("--------------------------------------------------------------------------------------------\n");
            printf("| %-17s | %-9s | %-8s | %-20s |\n", "Task Name", "Status", "Priority", "Stack High Water Mark");
            printf("--------------------------------------------------------------------------------------------\n");
            for (int i = 0; i < numTasks; i++) 
            {
                printf(
                    "| %-17s | %-9s | %-8u | %-20lu |\n",
                    taskStatusArray[i].pcTaskName,
                    (taskStatusArray[i].eCurrentState == eRunning ? "Running" : "Blocked"),
                    taskStatusArray[i].uxCurrentPriority,
                    taskStatusArray[i].usStackHighWaterMark
                );
            }
            printf("--------------------------------------------------------------------------------------------\n");
            printf("Current memory: %u, Lowest value: %u\n", (unsigned int)min_heap_atual, (unsigned int)min_heap);
            printf("--------------------------------------------------------------------------------------------\n");
            printf("Heap DRAM total: %d bytes\n", heap_caps_get_total_size(MALLOC_CAP_8BIT));
            printf("Heap IRAM total: %d bytes\n", heap_caps_get_total_size(MALLOC_CAP_32BIT));
            printf("--------------------------------------------------------------------------------------------\n");
            f_PrintHeapHistorico(min_heap_atual);

            vPortFree(taskStatusArray);
        }
        xSemaphoreGive(SemaphorePrintMem);
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
    hPrintMem = NULL;
    vTaskDelete(NULL); // Kill this task
}

void f_memoria(char *tarefa)
{
    INFO("Task memory usage (%s): %d bytes", tarefa, uxTaskGetStackHighWaterMark(NULL) * sizeof(StackType_t));
}

void f_PrintHeapHistorico(uint32_t atual) 
{
    heap_history[heap_index] = atual;
    heap_index = (heap_index + 1) % HIST_SIZE;
    if (heap_index == 0) 
    {
        heap_history_full = true;
    }

    uint32_t min = heap_history[0];
    uint32_t max = heap_history[0];
    uint8_t limite = heap_history_full ? HIST_SIZE : heap_index;

    for (int i = 1; i < limite; i++) 
    {
        if (heap_history[i] < min) min = heap_history[i];
        if (heap_history[i] > max) max = heap_history[i];
    }

    printf("Current memory: %lu, Min last 60s: %lu, Max last 60s: %lu\n", atual, min, max);
}

void f_HeapMonitor(void *pvParameter) 
{
    vTaskDelay(pdMS_TO_TICKS(60000)); // wait 1 minute before starting heap trace

    INFO("Starting heap trace...");
    heap_trace_init_standalone(trace_record, NUM_RECORDS);
    heap_trace_start(HEAP_TRACE_LEAKS);

    vTaskDelay(pdMS_TO_TICKS(600000)); // Accumulate data for 10 minutes
    vTaskDelay(pdMS_TO_TICKS(600000)); // Accumulate data for 10 minutes

    heap_trace_stop();
    INFO("Ending tracking. Leaks detected:");

    int count = heap_trace_get_count();

    for (int i = 0; i < count; i++) 
    {
        const heap_trace_record_t *rec = &trace_record[i];
        if (rec->address == NULL || rec->size == 0) continue;

        WARN("Allocation #%d: %u bytes in %p", i, rec->size, rec->address);

        for (int j = 0; j < CONFIG_HEAP_TRACING_STACK_DEPTH; j++) 
        {
            if (rec->alloced_by[j]) 
            {
                esp_rom_printf("    ↳ [%d] %p\n", j, rec->alloced_by[j]);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10)); // Make room for a watchdog
    }
    vTaskDelete(NULL);
}

#else

void debug_mem_start(void)
{
    ERROR("Heap tracing is not enabled. Please enable in menuconfig first.");
}

#endif /*CONFIG_HEAP_TRACING*/