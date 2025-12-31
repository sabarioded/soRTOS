#include "platform.h"
#include "system_clock.h"
#include "device_registers.h"
#include "led.h"
#include "utils.h"
#include "memory_map.h"
#include "systick.h"
#include "arch_ops.h"
#include "uart.h"

/* Default to MSI 4MHz (reset value) */
static size_t current_cpu_freq = 4000000; 

/* Initialize the platform core hardware. */
void platform_init(void) {
    /*
    * set up clock to 80 MHz, if fail panic
    * this function already takes care of flash and power config
    * according to the requested frequency 
    */
    if (system_clock_config_hz(SYSCLOCK_HZ_80MHZ) != SYSTEM_CLOCK_OK) {
        platform_panic();
    }

    current_cpu_freq = get_system_clock_hz();

    memory_map_init();

    /* To be added later - enable FP and use it */
    // volatile size_t *SCB_CPACR = (size_t *)0xE000ED88;
    // *SCB_CPACR |= ((3UL << 10*2)|(3UL << 11*2)); 
}

/* Enter a critical error state (Panic). */
void platform_panic(void) {
    /* Disable all interrupts to stop the system from doing anything else. */
    arch_irq_disable();

    led_init();
    
    while (1) {
        led_toggle();
        
        /* Busy wait delay */
        for(volatile int i=0; i<2000000; i++) {
            arch_nop();
        }
    }
}

/* Get the current CPU Core frequency in Hz. */
size_t platform_get_cpu_freq(void) {
    return current_cpu_freq;
}

/* Initializes the system tick timer. */
void platform_systick_init(size_t tick_hz) {
    systick_init(tick_hz);
}

/* Retrieves the current system tick count. */
size_t platform_get_ticks(void) {
    return systick_get_ticks();
}

/* Put the CPU into a low-power idle state. */
void platform_cpu_idle(void) {
    /*
     * Use the Wait For Interrupt instruction to put the CPU into a low-power
     * state. The CPU will wake up on the next interrupt.
     */
    arch_wfi();
}

/* Starts the scheduler. */
void platform_start_scheduler(size_t stack_pointer) {
    /* Forward declaration for the assembly function to start the first task. */
    extern void task_create_first(void);
     
    /* Set the process stack pointer */
    arch_set_psp(stack_pointer);

    /* Start the first task */
    task_create_first();
}

/* Initialize a task's stack frame. */
void *platform_initialize_stack(void *top_of_stack, void (*task_func)(void *), void *arg, void (*exit_handler)(void)) {
    return arch_initialize_stack(top_of_stack, task_func, arg, exit_handler);
}

/* Trigger a context switch. */
void platform_yield(void) {
    arch_yield();
}

/* Resets the system. */
void platform_reset(void) {
    arch_reset();
}

void platform_uart_init(void) {
    /* Initialize UART2 with buffered operation */
    UART_Config_t uart_config = {
        .BaudRate = 115200,
        .WordLength = UART_WORDLENGTH_8B,
        .Parity = UART_PARITY_NONE,
        .StopBits = UART_STOPBITS_1,
        .OverSampling8 = 0
    };
    
    /* USART2 is on APB1, which runs at the same frequency as SYSCLK (80 MHz) */
    uint32_t pclk1_hz = (uint32_t)platform_get_cpu_freq();
    uart_init(USART2, &uart_config, pclk1_hz);

    /* Enable UART2 interrupts in NVIC */
    NVIC_ISER1 |= (1UL << (USART2_IRQn & 0x1F));
    
    /* Enable RX interrupt for buffered reception */
    uart_enable_rx_interrupt(USART2, 1);
}

int platform_uart_getc(char *out_ch) {
    if (uart_available(USART2) > 0) {
        return uart_read_buffer(USART2, out_ch, 1);
    }
    return 0; /* No character available */
}

int platform_uart_puts(const char *s) {
    if (s == NULL) {
        return 0;
    }
    
    uint32_t len = 0;
    while (s[len] != '\0') {
        len++;
    }
    
    return uart_write_buffer(USART2, s, len);
}
