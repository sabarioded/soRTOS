#include <stdint.h>
#include <stddef.h>  /* for size_t */
#include "led.h"
#include "button.h"
#include "uart.h"
#include "systick.h"
#include "scheduler.h"
#include "cli.h"
#include "system_clock.h"
#include "device_registers.h"
#include "project_config.h"
#include "app_commands.h"
#if TASK_STACK_ALLOC_MODE == TASK_ALLOC_DYNAMIC
#include "heap.h"
/* Linker script symbols for heap */
extern uint8_t _end;              /* End of .bss section (start of heap) */
extern uint32_t __heap_limit__;   /* End of SRAM1 (end of heap) */
#endif


/* ---------- UART2 CLI Wrappers ---------- */

/* Non-blocking getc for CLI */
static int uart2_getc(char *out_ch)
{
    if (uart_available(USART2) > 0) {
        return uart_read_buffer(USART2, out_ch, 1);
    }
    return 0; /* No character available */
}

/* Non-blocking puts for CLI */
static int uart2_puts(const char *s)
{
    if (s == NULL) {
        return 0;
    }
    
    uint32_t len = 0;
    while (s[len] != '\0') {
        len++;
    }
    
    return uart_write_buffer(USART2, s, len);
}

/* ---------- Tasks ---------- */

static void task_blink(void *arg)
{
    (void)arg;
    led_init();

    while (1) {
        led_toggle();
        /* Sleep cooperatively for 500 SysTick ticks (e.g., 500 ms if 1 kHz) */
        task_sleep_ticks(500);
    }
}

static void task_button_logger(void *arg)
{
    (void)arg;
    button_init();

    uint8_t prev_btn = 0;
    while (1) {
        uint8_t btn = button_is_pressed();

        if (btn && !prev_btn) {
            cli_printf("Button pressed\r\n");
        } else if (!btn && prev_btn) {
            cli_printf("Button released\r\n");
        }

        prev_btn = btn;
        /* Poll button at 50 Hz without busy-waiting */
        task_sleep_ticks(20);
    }
}

/* ---------- main ---------- */

int main(void)
{
    /* Configure system clock to 80 MHz */
    system_clock_config_hz(SYSCLOCK_HZ_80MHZ);
    
    /* Initialize UART2 with buffered operation */
    UART_Config_t uart_config = {
        .BaudRate = 115200,
        .WordLength = UART_WORDLENGTH_8B,
        .Parity = UART_PARITY_NONE,
        .StopBits = UART_STOPBITS_1,
        .OverSampling8 = 0
    };
    
    /* USART2 is on APB1, which runs at the same frequency as SYSCLK (80 MHz) */
    uint32_t pclk1_hz = get_system_clock_hz();
    uart_init(USART2, &uart_config, pclk1_hz);
    
    /* Enable UART2 interrupts in NVIC */
    NVIC_ISER0 |= (1UL << (USART2_IRQn & 0x1F));
    
    /* Enable RX interrupt for buffered reception */
    uart_enable_rx_interrupt(USART2, 1);
    
    /* Enable TX interrupt for buffered transmission */
    uart_enable_tx_interrupt(USART2, 1);
    
#if TASK_STACK_ALLOC_MODE == TASK_ALLOC_DYNAMIC
    /* Initialize heap allocator for dynamic task stacks */
    void *heap_start_addr = (void*)&_end;
    size_t heap_size = (size_t)((uint32_t)&__heap_limit__ - (uint32_t)&_end);
    
    if (heap_init(heap_start_addr, heap_size) != 0) {
        /* Heap initialization failed - critical error */
        while(1) {
            __asm volatile ("nop");
        }
    }
#endif
    
    /* Initialize scheduler and SysTick (1 kHz tick) */
    scheduler_init();
    systick_init(1000);
    
    /* Initialize CLI */
    cli_init("OS> ", uart2_getc, uart2_puts);
    
    /* Register application commands */
    app_commands_register_all();
    
    /* Create application tasks */
    task_create(task_blink, NULL, STACK_SIZE_1KB);
    task_create(task_button_logger, NULL, STACK_SIZE_1KB);
    
    /* Create CLI task */
    task_create(cli_task_entry, NULL, STACK_SIZE_2KB);
    
    /* Start the scheduler - does not return */
    scheduler_start();

    /* Should never reach here */
    while (1) {
        __asm volatile ("nop");
    }
}
