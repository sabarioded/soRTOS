#include "console.h"
#include "platform.h"
#include "uart.h"
#include "utils.h"

static uart_port_t console_uart;

int console_init(void) {
    console_uart = platform_uart_init();
    return (console_uart != NULL) ? 1 : 0;
}

int console_has_uart(void) {
    return (console_uart != NULL) ? 1 : 0;
}

int console_getc(char *out_ch) {
    if (!out_ch) {
        return 0;
    }
    if (console_uart) {
        return uart_read_buffer(console_uart, out_ch, 1);
    }
    return platform_uart_getc(out_ch);
}

int console_puts(const char *s) {
    if (!s) {
        return 0;
    }
    if (console_uart) {
        return uart_write_buffer(console_uart, s, utils_strlen(s));
    }
    return platform_uart_puts(s);
}

void console_attach_queues(queue_t *rx, queue_t *tx) {
    if (!console_uart) {
        return;
    }
    uart_set_rx_queue(console_uart, rx);
    uart_set_tx_queue(console_uart, tx);
}
