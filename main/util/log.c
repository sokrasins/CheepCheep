#include "log.h"

void log_global_level_set(log_level_t level)
{
    switch (level)
    {
        case LOG_ERROR:
            esp_log_level_set("*", ESP_LOG_ERROR);
            break;
        case LOG_WARNING:
            esp_log_level_set("*", ESP_LOG_WARN);
            break;
        case LOG_INFO:
            esp_log_level_set("*", ESP_LOG_INFO); 
            break;
        case LOG_DEBUG:
            esp_log_level_set("*", ESP_LOG_DEBUG); 
            break;
        case LOG_VERBOSE:
            esp_log_level_set("*", ESP_LOG_VERBOSE); 
            break; 
    }
}