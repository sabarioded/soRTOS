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

/* ---------- main ---------- */

int main(void)
{
    platform_init();

    platform_uart_init();
    
    /* Initialize scheduler */
    scheduler_init();
    
    /* Initialize CLI */
    cli_init("OS> ", platform_uart_getc, platform_uart_puts);
    
    /* Register application commands */
    app_commands_register_all();
    
    /* Create CLI task */
    task_create(cli_task_entry, NULL, STACK_SIZE_2KB);
    
    /* Start the scheduler - does not return */
    scheduler_start();
    
    /* Should never reach here */
    while (1) {
        arch_nop();
    }
}
