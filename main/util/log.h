#ifndef LOG_H_
#define LOG_H_

#include "esp_log.h"

// Quick macros for logging. This uses the filename of the including source as 
// the tag. This is more convenient for getting this up fast, but is annoying 
// for control (filename isn't a good tag for changing logging levels of 
// specific contexts).
#define ERROR(format, ...)   ESP_LOGE(__FILE__, format, ##__VA_ARGS__)
#define WARN(format, ...)    ESP_LOGW(__FILE__, format, ##__VA_ARGS__)
#define INFO(format, ...)    ESP_LOGI(__FILE__, format, ##__VA_ARGS__)
#define DEBUG(format, ...)   ESP_LOGD(__FILE__, format, ##__VA_ARGS__)
#define VERBOSE(format, ...) ESP_LOGV(__FILE__, format, ##__VA_ARGS__)

typedef enum {
    LOG_ERROR,
    LOG_WARNING,
    LOG_INFO,
    LOG_DEBUG,
    LOG_VERBOSE,
} log_level_t;

/**
 * @brief Sets the global logging level
 * @param level logging level to limit all logs to
 */
void log_global_level_set(log_level_t level);

#endif /*LOG_H_*/