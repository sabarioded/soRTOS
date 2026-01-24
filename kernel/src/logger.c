#include "logger.h"
#include "queue.h"
#include "scheduler.h"
#include "platform.h"
#include "cli.h"
#include "utils.h"

#if LOG_ENABLE

static queue_t *log_queue = NULL;

/* Circular buffer for log history */
static log_entry_t log_history[LOG_HISTORY_SIZE];
static uint32_t log_head = 0;   /* Write index */
static uint32_t log_count = 0;  /* Total items in buffer */
static uint8_t log_live = 0;    /* 0 = Saved only, 1 = Print immediately */


/* Low priority task that waits for log entries and prints them. */
static void logger_task_entry(void *arg) {
    (void)arg;
    log_entry_t entry;

    while (1) {
        /* Block until a log entry arrives */
        if (queue_pop(log_queue, &entry) == 0) {
            
            /* Save to history buffer */
            log_history[log_head] = entry;
            log_head = (log_head + 1) % LOG_HISTORY_SIZE;
            if (log_count < LOG_HISTORY_SIZE) {
                log_count++;
            }

            /* If live mode is enabled, print immediately */
            if (log_live) {
                uint32_t time_sec = entry.timestamp / 1000;
                uint32_t time_ms = entry.timestamp % 1000;
                
                cli_printf("[%u.%03u] ", time_sec, time_ms);
                cli_printf(entry.fmt, entry.arg1, entry.arg2);
                cli_printf("\r\n");
            }
        }
    }
}

/* CLI Command Handler: log [dump|live|clear] */
static int cmd_log_handler(int argc, char **argv) {
    if (argc < 2 || utils_strcmp(argv[1], "dump") == 0) {
        /* Dump the history buffer */
        cli_printf("--- Log History (%u entries) ---\r\n", log_count);
        
        /* Calculate start index (oldest entry) */
        uint32_t idx = (log_head + LOG_HISTORY_SIZE - log_count) % LOG_HISTORY_SIZE;
        
        for (uint32_t i = 0; i < log_count; i++) {
            log_entry_t *e = &log_history[idx];
            
            uint32_t time_sec = e->timestamp / 1000;
            uint32_t time_ms = e->timestamp % 1000;
            
            cli_printf("[%u.%03u] ", time_sec, time_ms);
            cli_printf(e->fmt, e->arg1, e->arg2);
            cli_printf("\r\n");
            
            idx = (idx + 1) % LOG_HISTORY_SIZE;
        }
        cli_printf("--- End ---\r\n");
        return 0;
    }
    
    if (utils_strcmp(argv[1], "live") == 0) {
        if (argc >= 3) {
            if (utils_strcmp(argv[2], "on") == 0) log_live = 1;
            else if (utils_strcmp(argv[2], "off") == 0) log_live = 0;
        }
        cli_printf("Live Logging: %s\r\n", log_live ? "ON" : "OFF");
        return 0;
    }

    if (utils_strcmp(argv[1], "clear") == 0) {
        log_count = 0;
        log_head = 0;
        cli_printf("Log cleared.\r\n");
        return 0;
    }

    cli_printf("Usage: log [dump|live <on/off>|clear]\r\n");
    return 0;
}

static const cli_command_t log_cmd = {
    .name = "log",
    .help = "Manage system logs",
    .handler = cmd_log_handler
};

/* Initialize the logger */
void logger_init(void) {
    /* Create a queue to hold binary log events */
    log_queue = queue_create(sizeof(log_entry_t), LOG_QUEUE_SIZE);
    
    if (log_queue) {
        /* Create the logger task with LOW priority */
        task_create(logger_task_entry, NULL, STACK_SIZE_1KB, TASK_WEIGHT_LOW);
        
        /* Register the CLI command */
        cli_register_command(&log_cmd);
    }
}

/* Push a log entry to the queue */
void logger_log(const char *fmt, uintptr_t arg1, uintptr_t arg2) {
    if (!log_queue) return;

    log_entry_t entry;
    entry.timestamp = (uint32_t)platform_get_ticks();
    entry.fmt = fmt;
    entry.arg1 = arg1;
    entry.arg2 = arg2;

    /* Use ISR version to avoid blocking */
    queue_push_from_isr(log_queue, &entry);
}

/* Get the logger queue for debugging */
queue_t* logger_get_queue(void) {
    return log_queue;
}


#endif /* LOG_ENABLE */