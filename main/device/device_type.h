#ifndef DEVICE_TYPE_H_
#define DEVICE_TYPE_H_

#include "status.h"
#include "config.h"

typedef status_t (*init_t)(const config_t *config);
typedef status_t (*deinit_t)(void);

typedef struct {
    init_t init;
    deinit_t deinit;
} device_t;

#endif /*DEVICE_TYPE_H_*/