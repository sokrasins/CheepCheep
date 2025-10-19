#ifndef DEVICE_H_
#define DEVICE_H_

#include "device_door.h"
#include "device_interlock.h"
#include "device_vending.h"

typedef status_t (*init_t)(const config_t *config);
typedef status_t (*deinit_t)(void);

typedef struct {
    init_t init;
    deinit_t deinit;
} device_t;

#endif /*DEVICE_H_*/