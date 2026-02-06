#ifndef CONSOLE_H
#define CONSOLE_H

#include "queue.h"

/**
 * @brief Initialize the console backend.
 * @return 1 if a driver-backed UART is available, 0 otherwise.
 */
int console_init(void);

/**
 * @brief Check if the console is backed by a driver UART.
 * @return 1 if driver-backed UART is available, 0 otherwise.
 */
int console_has_uart(void);

/**
 * @brief Read a single character from the console (non-blocking).
 * @param out_ch Output character pointer.
 * @return 1 if a character was read, 0 otherwise.
 */
int console_getc(char *out_ch);

/**
 * @brief Write a string to the console (non-blocking).
 * @param s Null-terminated string to write.
 * @return Number of bytes written.
 */
int console_puts(const char *s);

/**
 * @brief Attach RX/TX queues to the console UART (no-op if no UART).
 * @param rx Receive queue.
 * @param tx Transmit queue.
 */
void console_attach_queues(queue_t *rx, queue_t *tx);

#endif /* CONSOLE_H */
