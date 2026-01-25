#ifndef CLI_H
#define CLI_H

#include <stdint.h>
#include "queue.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Return codes */
typedef enum {
    CLI_OK              = 0,
    CLI_ERR             = -1,
    CLI_ERR_NOCMD       = -2,
    CLI_ERR_TOOLONG     = -3,
    CLI_ERR_BAD_ARGS    = -4,
    CLI_ERR_BUSY        = -5
} cli_status_t;

/* Callback for command handlers 
 * argc: argument count (includes command name)
 * argv: array of string pointers (argv[0] is command name)
 */
typedef int (*cli_cmd_fn_t)(int argc, char **argv);

/* IO Abstraction callbacks
 * getc: Reads 1 char. Returns 1 if char read, 0 if no char available (non-blocking).
 * puts: Writes string. Returns number of bytes written.
 */
typedef int (*cli_getc_fn_t)(char *out_ch); 
typedef int (*cli_puts_fn_t)(const char *s); 

/* Command definition structure */
typedef struct {
    const char     *name;    /* Command name (no spaces allowed) */
    const char     *help;    /* Short help string */
    cli_cmd_fn_t    handler; /* Function pointer to handler */
} cli_command_t;


/**
 * @brief Initialize the CLI subsystem.
 * @param prompt String to display when waiting for input (e.g., "OS> ")
 * @param getc Function pointer to non-blocking character read
 * @param puts Function pointer to string write
 * @return CLI_OK on success
 */
int32_t cli_init(const char *prompt, cli_getc_fn_t getc, cli_puts_fn_t puts);


/**
 * @brief Register a command with the CLI.
 * @param cmd Pointer to the command structure (must persist in memory)
 * @return CLI_OK on success, CLI_ERR if table full
 */
int32_t cli_register_command(const cli_command_t *cmd);


/**
 * @brief Unregister a command by name.
 * @param name Command name string
 * @return CLI_OK on success, CLI_ERR_NOCMD if not found
 */
int32_t cli_unregister_command(const char *name);


/**
 * @brief Main task loop. Pass this function to task_create().
 * This function loops forever and handles sleeping/input.
 */
void cli_task_entry(void *arg);


/**
 * @brief printf-style wrapper for CLI output.
 * Formats string and sends it via the registered 'puts' function.
 */
uint32_t cli_printf(const char *fmt, ...);


/**
 * @brief Set the input queue for the CLI.
 * @param q Pointer to the queue
 */
void cli_set_rx_queue(queue_t *q);


/**
 * @brief Set the output queue for the CLI.
 * If set, output will be pushed to this queue instead of using the puts callback.
 * @param q Pointer to the queue.
 */
void cli_set_tx_queue(queue_t *q);


#ifdef __cplusplus
}
#endif

#endif /* CLI_H */