#ifndef CONSOLE_H_
#define CONSOLE_H_

#include "esp_console.h"
#include "status.h"

/**
 * @brief cli command handler, registered by consumers of cli commands
 * @param argc number of arguments in argv
 * @param argv command arguments
 * @return TODO define this behavior
 */
typedef int (*cli_cb_t)(int argc, char **argv);

/**
 * 
 */
status_t console_start(void);

status_t console_register(char *cmd, char *help, char *hint, cli_cb_t cb);

#endif /*CONSOLE_H_*/