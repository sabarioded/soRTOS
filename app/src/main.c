#include <stdint.h>
#include <stddef.h>
#include "cli.h"
#include "project_config.h"
#include "app_commands.h"
#include "platform.h"
#include "scheduler.h"
#include "arch_ops.h"
#include "queue.h"
#include "logger.h"
#include "console.h"

int main(void)
{
    platform_init();
    
    /* Initialize scheduler */
    scheduler_init();

    /* Initialize console backend (driver-backed if available) */
    console_init();
    
    /* Initialize CLI */
    cli_init("soRTOS> ", console_getc, console_puts);
    
    if (console_has_uart()) {
        /* Create queues for UART-backed CLI I/O */
        queue_t *cli_rx_queue = queue_create(sizeof(char), 128);
        queue_t *cli_tx_queue = queue_create(sizeof(char), 128);
        if (!cli_rx_queue || !cli_tx_queue) {
            platform_panic();
        }

        console_attach_queues(cli_rx_queue, cli_tx_queue);
        cli_set_rx_queue(cli_rx_queue);
        cli_set_tx_queue(cli_tx_queue);
    }

    /* Initialize Logger (creates log task) */
    logger_init();

    /* Register application commands */
    app_commands_register_all();
    
    /* Create CLI task */
    int32_t cli_task_id = task_create(cli_task_entry, NULL, STACK_SIZE_2KB, TASK_WEIGHT_NORMAL);
    
    /* Start the scheduler - does not return */
    scheduler_start();

#ifdef HOST_PLATFORM
    if (cli_task_id > 0) {
        task_t *cli_task = NULL;
        for (uint32_t i = 0; i < MAX_TASKS; i++) {
            task_t *t = scheduler_get_task_by_index(i);
            if (t && task_get_id(t) == (uint16_t)cli_task_id) {
                cli_task = t;
                break;
            }
        }
        if (cli_task) {
            task_set_current(cli_task);
        }
    }
    /* Native platform has no context switching. run CLI loop on main thread. */
    cli_task_entry(NULL);
#endif

    /* Should never reach here */
    while (1) {
        arch_nop();
    }
}
