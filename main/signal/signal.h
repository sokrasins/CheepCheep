#ifndef BUZZER_H_
#define BUZZER_H_

#include "status.h"
#include "config.h"

/**
 * @brief Initialize the signal controller. This module allows signaling on a wiegand card reader.
 * @param config the signal-specific configuration
 * @return STATUS_OK: successful
 */
status_t signal_init(const config_buzzer_t *config);

/**
 * @brief Do "alert" signal
 */
void signal_alert(void);

/**
 * @brief Do "ok" signal
 */
void signal_ok(void);

/**
 * @brief Do "cardread" signal
 */
void signal_cardread(void);

/**
 * @brief Do "action" signal
 */
void signal_action(void);

#endif /*BUZZER_H_*/