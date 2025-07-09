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
 * @brief Start the console. This is done after all components have registered 
 * their commands.
 * @return STATUS_OK: success
 */
status_t console_start(void);

/**
 * @brief Register a new command to the console
 * @param cmd the string that must be typed on the cli for the new command
 * @param help text displayed in help to describe command usage. This can be 
 * NULL.
 * @param hint list of all args the cmd can take. This can be NULL.
 * @param cb the callback that handles argument parsing and command handling.
 * @return STATUS_OK: success
 */
status_t console_register(char *cmd, char *help, char *hint, cli_cb_t cb);

#endif /*CONSOLE_H_*/