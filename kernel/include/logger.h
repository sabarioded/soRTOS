#ifndef LOGGER_H
#define LOGGER_H

#include <stdint.h>
#include "project_config.h"
#include "queue.h"

#if LOG_ENABLE

typedef struct {
    uint32_t timestamp;
    const char *fmt; /* Format string pointer (not string itself)*/
    uintptr_t arg1; /* argument to be inserted in string */
    uintptr_t arg2; /* argument to be inserted in string */
} log_entry_t;

/**
 * @brief Initialize the logger system and background task.
 */
void logger_init(void);

/**
 * @brief Log a message (Deferred).
 * This function is fast, non-blocking, and safe to call from ISRs.
 */
void logger_log(const char *fmt, uintptr_t arg1, uintptr_t arg2);

/**
 * @brief Get the logger queue for debugging.
 */
queue_t* logger_get_queue(void);

#else
/* Compile out logging if disabled */
#define logger_init()
#endif

#endif /* LOGGER_H */