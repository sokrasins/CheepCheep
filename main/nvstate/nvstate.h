#ifndef NVSTATE_H_
#define NVSTATE_H_

#include "status.h"
#include "config.h"
#include <stddef.h>
#include <stdbool.h>

// Expected number of bytes in tag_hash
#define TAG_HASH_LEN 16U

/**
 * @brief Initialize non-volatile state driver
 * @return -STATUS_IO: issue opening the nvs area
 *          STATUS_OK: successful
 */
status_t nvstate_init(void);

/**
 * @brief Get lockout status. This indicates if the door lock is explicitly 
 * locked out by the server (and shouldn't open on a successful badge read)
 * @return true if "locked out", false if not.
 *         If an error is encountered while reading the setting, true is 
 *         returned
 */
status_t nvstate_locked_out(bool *locked_out);

/**
 * @brief Set the locked out status.
 * @param locked_out true if locked out, false otherwise
 * @return STATUS_OK: successful
 */
status_t nvstate_locked_out_set(bool locked_out);

/**
 * @brief Get the current hash of the list of authorized tags. 
 * @param tag_hash memory for the returned tag_hash (must be at least 
 * TAG_HASH_LEN bytes in size)
 * @param len number of bytes written to tag_hash
 * @return -STATUS_NO_RESOURCE: Unable to find the tag hash in flash
 *          STATUS_OK: successful
 */
status_t nvstate_tag_hash(uint8_t *tag_hash, size_t *len);

/**
 * @brief Set the current hash of the list of authorized tags.
 * @param tag_hash new value of tag_hash
 * @param len number of bytes in tag_hash. Should be TAG_HASH_LEN.
 * @return STATUS_OK: successful
 */
status_t nvstate_tag_hash_set(uint8_t *tag_hash, size_t len);

/** 
 * @brief Get the current stored config
 * @param config stored config
 * @return -STATUS_NO_RESOURCE: Unable to find a config stored in nvs
 *          STATUS_OK: successful
*/
status_t nvstate_config(config_t *config);

/**
 * @brief Set the current config
 * @param config config to store
 * @return -STATUS_NO_RESOURCE: Unable to find a config stored in nvs
 *          STATUS_OK: successful
 */
status_t nvstate_config_set(const config_t *config);

#endif /*NVSTATE_H_*/