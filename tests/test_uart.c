#include "unity.h"
#include "uart.h"
#include "queue.h"
#include "mock_drivers.h"
#include "allocator.h"
#include "test_common.h"
#include "spinlock.h"
#include <stdio.h>

/* Partial definition to access handle in tests */
struct uart_context {
    void *hal_handle;
    uint8_t *rx_buf;
    uint8_t *tx_buf;
    queue_t *rx_queue;
    queue_t *tx_queue;
    spinlock_t lock;
    uint16_t rx_buf_size;
    uint16_t tx_buf_size;
    volatile uint16_t rx_head;
    volatile uint16_t rx_tail;
    volatile uint16_t tx_head;
    volatile uint16_t tx_tail;
};

static void setUp_local(void) {
    mock_drivers_reset();
}

static void tearDown_local(void) {
}

void test_uart_create_should_AllocateAndInit(void) {
    void* hal_handle = (void*)0x1000;
    void* config = (void*)0x2000;
    uint8_t rx_buf[10];
    uint8_t tx_buf[10];

    uart_port_t port = uart_create(hal_handle, rx_buf, 10, tx_buf, 10, config, 80000000);
    TEST_ASSERT_NOT_NULL(port);
    TEST_ASSERT_EQUAL_PTR(hal_handle, port->hal_handle);
}

void test_uart_write_buffer_should_CopyAndEnableTx(void) {
    struct uart_context ctx;
    uint8_t tx_buf[10];
    ctx.tx_buf = tx_buf;
    ctx.tx_buf_size = 10;
    ctx.tx_head = 0;
    ctx.tx_tail = 0;
    ctx.hal_handle = (void*)0x1000;

    spinlock_init(&ctx.lock);

    const char *data = "Hi";
    int written = uart_write_buffer(&ctx, data, 2);
    
    TEST_ASSERT_EQUAL(2, written);
    TEST_ASSERT_EQUAL('H', tx_buf[0]);
    TEST_ASSERT_EQUAL('i', tx_buf[1]);
    TEST_ASSERT_EQUAL(1, mock_uart_enable_tx_irq_arg);
}

static void uart_dma_done(void *arg) {
    int *counter = (int *)arg;
    (*counter)++;
}

void test_uart_dma_should_InvokeCallback(void) {
    void *hal_handle = (void*)0x1000;
    void *config = (void*)0x2000;
    uint8_t rx_buf[8];
    uint8_t tx_buf[8];
    size_t ctx_size = uart_get_context_size();
    uint8_t ctx_mem[ctx_size];
    int tx_done = 0;
    int rx_done = 0;
    const uint8_t data[] = {0xAA, 0xBB, 0xCC};

    uart_port_t port = uart_init(ctx_mem, hal_handle, rx_buf, sizeof(rx_buf), tx_buf, sizeof(tx_buf), config, 80000000);
    TEST_ASSERT_NOT_NULL(port);

    TEST_ASSERT_EQUAL(0, uart_write_dma(port, data, sizeof(data), uart_dma_done, &tx_done));
    TEST_ASSERT_EQUAL(1, tx_done);

    TEST_ASSERT_EQUAL(0, uart_read_dma(port, rx_buf, sizeof(rx_buf), uart_dma_done, &rx_done));
    TEST_ASSERT_EQUAL(1, rx_done);
}

void run_uart_tests(void) {
    printf("\n=== Starting UART Tests ===\n");

    test_setUp_hook = setUp_local;
    test_tearDown_hook = tearDown_local;
    UnitySetTestFile("tests/test_uart.c");
    RUN_TEST(test_uart_create_should_AllocateAndInit);
    RUN_TEST(test_uart_write_buffer_should_CopyAndEnableTx);
    RUN_TEST(test_uart_dma_should_InvokeCallback);

    printf("=== UART Tests Complete ===\n");
}
