#ifndef DEVICE_TYPE_API_H_
#define DEVICE_TYPE_API_H_

#include "config_types.h"
#include "status.h"

/**
 * @brief Initialize the device
 * @param config device configuration
 * @return STATUS_OK: successful
 */
typedef status_t (*device_init_t)(const config_t *config);

// Device object
typedef struct {
    device_init_t init;
} device_t;

#endif /*DEVICE_TYPE_API_H_*/