#include <stdint.h>
#include <stddef.h>
#include "utils.h"
#include "cli.h"
#include "project_config.h"
#include "app_commands.h"
#include "platform.h"
#include "memory_map.h"
#include "scheduler.h"
#include "arch_ops.h"
#include "queue.h"
#include "logger.h"


int main(void)
{
    platform_init();

    platform_uart_init();
    
    /* Initialize scheduler */
    scheduler_init();

    /* Initialize Logger (creates log task) */
    logger_init();
    
    /* Initialize CLI */
    cli_init("soRTOS> ", platform_uart_getc, platform_uart_puts);
    
    /* Register application commands */
    app_commands_register_all();
    
    /* Create a queue for CLI input */
    queue_t *cli_rx_queue = queue_create(sizeof(char), 128);

    /* Create a queue for CLI output */
    queue_t *cli_tx_queue = queue_create(sizeof(char), 128);
    
    /* Register queue with UART driver and CLI */
    platform_uart_set_rx_queue(cli_rx_queue);
    cli_set_rx_queue(cli_rx_queue);

    platform_uart_set_tx_queue(cli_tx_queue);
    cli_set_tx_queue(cli_tx_queue);
    
    /* Create CLI task */
    task_create(cli_task_entry, NULL, STACK_SIZE_2KB, TASK_WEIGHT_NORMAL);
    
    /* Start the scheduler - does not return */
    scheduler_start();
    
    /* Should never reach here */
    while (1) {
        arch_nop();
    }
}
