#ifndef APP_COMMANDS_H
#define APP_COMMANDS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Register all application CLI commands
 * 
 * This function registers commands like heap statistics,
 * task information, etc.
 */
void app_commands_register_all(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_COMMANDS_H */

