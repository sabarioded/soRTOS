#include "platform.h"
#include "memory_map.h"
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

/* --- Timekeeping --- */
static struct timespec start_time;

/* --- Terminal Settings --- */
static struct termios orig_termios;
static int term_saved = 0;

/* Restore terminal settings on exit */
static void restore_terminal(void) {
    if (term_saved) {
        tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
    }
}

/* --- Platform API Implementation --- */

void platform_init(void) {
    /* Initialize the monotonic clock reference */
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    /* Initialize memory map (Heap) */
    memory_map_init();
}

void platform_uart_init(void) {
    /* 
     * Configure the host terminal to behave like a raw UART:
     * 1. Disable Canonical Mode (Input is available immediately, not line-by-line)
     * 2. Disable Echo (The CLI handles echoing)
     */
    if (tcgetattr(STDIN_FILENO, &orig_termios) == 0) {
        term_saved = 1;
        atexit(restore_terminal);

        struct termios t = orig_termios;
        t.c_lflag &= ~(ICANON | ECHO); 
        tcsetattr(STDIN_FILENO, TCSANOW, &t);
    }

    /* Set STDIN to Non-Blocking mode so polling doesn't freeze the CPU */
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    
    /* Disable buffering on stdout to ensure prints appear immediately */
    setvbuf(stdout, NULL, _IONBF, 0);
}

int platform_uart_getc(char *out_ch) {
    /* Try to read one character from stdin (non-blocking) */
    char ch;
    ssize_t n = read(STDIN_FILENO, &ch, 1);
    
    if (n > 0) {
        *out_ch = (char)ch;
        return 1; /* Char received */
    }
    return 0; /* No char available */
}

int platform_uart_puts(const char *s) {
    /* Just print to stdout */
    return printf("%s", s);
}

void platform_panic(void) {
    printf("[PANIC] System halted.\n");
    exit(1);
}

size_t platform_get_cpu_freq(void) { 
    return 0; /* Not relevant on host */
}

void platform_systick_init(size_t tick_hz) { (void)tick_hz; }

size_t platform_get_ticks(void) { 
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    
    /* Convert to milliseconds */
    size_t diff_ms = (now.tv_sec - start_time.tv_sec) * 1000 + 
                     (now.tv_nsec - start_time.tv_nsec) / 1000000;
    return diff_ms;
}

void platform_cpu_idle(void) { 
    /* Sleep 1ms to save host CPU usage */
    struct timespec ts = {0, 1000000}; 
    nanosleep(&ts, NULL);
}

void platform_start_scheduler(size_t stack_pointer) { 
    (void)stack_pointer;
    printf("[INFO] Native scheduler started.\n[WARN] Context switching not implemented on native target.\n[WARN] Only the main thread (CLI) will run.\n");
}

void platform_yield(void) { }
void platform_reset(void) { exit(0); }

void *platform_initialize_stack(void *top_of_stack, void (*task_func)(void *), void *arg, void (*exit_handler)(void)) {
    (void)task_func; (void)arg; (void)exit_handler;
    return top_of_stack;
}