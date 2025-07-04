#ifndef TAGS_H_
#define TAGS_H_

#include "status.h"
#include <stddef.h>

/**
 * @brief Initialize the verified cards database.
 * @return -STATUS_NOFILE: Couldn't open or create the tags file
 *         -STATUS_NOMEM: Couldn't register handler with the client
 *          STATUS_OK: Successful
 */
status_t tags_init(void);

/**
 * @brief Verify if the provided card is preset in the card database
 * @param card data to find
 * @return -STATUS_INVALID: card unauthorized, not in database
 *          STATUS_OK: card authorized
 */
status_t tags_verify(uint32_t card);

#endif /*TAGS_H_*/