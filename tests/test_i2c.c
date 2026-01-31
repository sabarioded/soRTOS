#include "unity.h"
#include "i2c.h"
#include "mock_drivers.h"
#include "i2c_hal.h"
#include "allocator.h"
#include "test_common.h"
#include <stdio.h>

static void setUp_local(void) {
    mock_drivers_reset();
}

static void tearDown_local(void) {
}

static void i2c_dma_done(void *arg) {
    int *counter = (int *)arg;
    (*counter)++;
}

void test_i2c_create_should_InitHal(void) {
    I2C_TypeDef regs;
    void* hal_handle = &regs;
    void* config = (void*)0x3000;

    i2c_port_t port = i2c_create(hal_handle, config);
    TEST_ASSERT_NOT_NULL(port);
    TEST_ASSERT_EQUAL(1, mock_i2c_init_called);
}

void test_i2c_master_transmit_should_CallHal(void) {
    I2C_TypeDef regs;
    void *hal_handle = &regs;
    void *config = (void*)0x3000;
    size_t ctx_size = i2c_get_context_size();
    uint8_t ctx_mem[ctx_size];
    i2c_port_t port = i2c_init(ctx_mem, hal_handle, config);
    
    uint8_t data[] = {0x01, 0x02};
    
    mock_i2c_transmit_return = 0;
    
    int res = i2c_master_transmit(port, 0x50, data, 2);
    TEST_ASSERT_EQUAL(0, res);
}

void test_i2c_master_receive_should_CallHal(void) {
    I2C_TypeDef regs;
    void *hal_handle = &regs;
    void *config = (void*)0x3000;
    size_t ctx_size = i2c_get_context_size();
    uint8_t ctx_mem[ctx_size];
    i2c_port_t port = i2c_init(ctx_mem, hal_handle, config);
    
    uint8_t buf[2];
    
    mock_i2c_receive_return = 0;
    
    int res = i2c_master_receive(port, 0x50, buf, 2);
    TEST_ASSERT_EQUAL(0, res);
}

void test_i2c_dma_should_InvokeCallback(void) {
    I2C_TypeDef regs;
    void *hal_handle = &regs;
    void *config = (void*)0x3000;
    size_t ctx_size = i2c_get_context_size();
    uint8_t ctx_mem[ctx_size];
    i2c_port_t port = i2c_init(ctx_mem, hal_handle, config);
    uint8_t data[] = {0x01, 0x02, 0x03};
    uint8_t buf[3];
    int tx_done = 0;
    int rx_done = 0;

    mock_i2c_transmit_return = 0;
    mock_i2c_receive_return = 0;

    TEST_ASSERT_EQUAL(0, i2c_master_transmit_dma(port, 0x50, data, sizeof(data), i2c_dma_done, &tx_done));
    TEST_ASSERT_EQUAL(1, tx_done);

    TEST_ASSERT_EQUAL(0, i2c_master_receive_dma(port, 0x50, buf, sizeof(buf), i2c_dma_done, &rx_done));
    TEST_ASSERT_EQUAL(1, rx_done);
}

void run_i2c_tests(void) {
    printf("\n=== Starting I2C Tests ===\n");

    test_setUp_hook = setUp_local;
    test_tearDown_hook = tearDown_local;
    UnitySetTestFile("tests/test_i2c.c");
    RUN_TEST(test_i2c_create_should_InitHal);
    RUN_TEST(test_i2c_master_transmit_should_CallHal);
    RUN_TEST(test_i2c_master_receive_should_CallHal);
    RUN_TEST(test_i2c_dma_should_InvokeCallback);

    printf("=== I2C Tests Complete ===\n");
}
