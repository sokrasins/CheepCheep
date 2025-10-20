#ifndef DEVICE_H_
#define DEVICE_H_

#include "device_door.h"
#include "device_interlock.h"
#include "device_vending.h"

static inline const device_t *device_get(device_type_t type)
{
    const device_t *device = NULL;

    switch (type)
    {
        case DEVICE_DOOR:
            device = &door;
            break;

        case DEVICE_INTERLOCK:
            device = &ilock;
            break;

        case DEVICE_VENDING:
            device = &vending;
            break;

        default:
            // When there's no match, the value of device remains NULL
    }

    return device;
}

#endif /*DEVICE_H_*/