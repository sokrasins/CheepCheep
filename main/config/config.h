#ifndef CONFIG_H_
#define CONFIG_H_

#include "config_types.h"
#include "status.h"

#include <stdbool.h>
#include <string.h>

/**
 * @brief Retrieve the device config
 * @return NULL if error getting the config
 *         Otherwise, pointer to device config
 */
const config_t *config_get(void);

#endif /*CONFIG_H_*/
