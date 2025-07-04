#ifndef WIEGAND_H_
#define WIEGAND_H_

#include "status.h"
#include <stdint.h>
#include <stdbool.h>


typedef enum {
    WIEG_EVT_NEWCARD,   // New (valid) card is received
    WIEG_EVT_NEWBIT,    // Unimplemented. A single new bit is received
} wieg_evt_t;

typedef enum {
    WIEG_24_BIT,        // 8 bit facility + 16 bit user id     
    WIEG_32_BIT,        // 16 bit facility + 16 bit user id
} wieg_encoding_t;

typedef union {
    uint32_t raw;           // Unparsed card data. This is how membermatters reports cards.
    struct __attribute__((packed)) {
        uint16_t user_id;
        uint16_t facility;  // If 24-bit mode is selected, this value won't exceed 0xFF  
    };
} card_t;

// Event handle, necessary for deregistering the event
typedef void *wieg_evt_handle_t;

/**
 * @brief Callback for wiegand events. This is how other modules receive card 
 * swipes.
 * @param event Reason the callback is executed
 * @param card Reported card data
 * @param ctx Context provided by the registering code
 */
typedef void (*wieg_evt_cb_t)(wieg_evt_t event, card_t *card, void *ctx);

/**
 * @brief Initialize the wiegand reader
 * @param d0 GPIO number (not physical pin number) of the d0 signal. This pin does not have to be configured.
 * @param d1 GPIO number (not physical pin number) of the d1 signal. This pin does not have to be configured.
 * @param encode card encoding to parse
 * @return -STATUS_UNIMPL: Implemented config was selected
 *          STATUS_OK: Successful
 */
status_t wieg_init(int d0, int d1, wieg_encoding_t encode);

/**
 * @brief Register an event handler for one of the wiegand events
 * @param event cb called for all occurrences of the event specified here
 * @param cb 
 * @param ctx context to pass to the callback
 * @return NULL if the callback couldn't be registered.
 */
wieg_evt_handle_t wieg_evt_handler_reg(wieg_evt_t event, wieg_evt_cb_t cb, void *ctx);

/**
 * @brief Deregister an event handler
 * @param handle
 * @return STATUS_OK: Successful
 */
status_t wieg_evt_handler_dereg(wieg_evt_handle_t handle);

#endif /*WIEGAND_H_*/