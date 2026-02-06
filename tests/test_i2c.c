#include "unity.h"
#include "i2c.h"
#include "mock_drivers.h"
#include "i2c_hal.h"
#include "allocator.h"
#include "test_common.h"
#include <stdio.h>

static int i2c_cb_called = 0;
static i2c_status_t i2c_cb_status = I2C_STATUS_OK;

static void setUp_local(void) {
    mock_drivers_reset();
    i2c_cb_called = 0;
    i2c_cb_status = I2C_STATUS_OK;
}

static void tearDown_local(void) {
}

static void i2c_test_callback(void *arg, i2c_status_t status) {
    (void)arg;
    i2c_cb_called++;
    i2c_cb_status = status;
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

void test_i2c_master_transmit_should_Fail_OnNullData(void) {
    I2C_TypeDef regs;
    void *hal_handle = &regs;
    void *config = (void*)0x3000;
    size_t ctx_size = i2c_get_context_size();
    uint8_t ctx_mem[ctx_size];
    i2c_port_t port = i2c_init(ctx_mem, hal_handle, config);

    int res = i2c_master_transmit(port, 0x50, NULL, 2);
    TEST_ASSERT_EQUAL(-1, res);
}

void test_i2c_master_receive_should_Fail_OnNullBuffer(void) {
    I2C_TypeDef regs;
    void *hal_handle = &regs;
    void *config = (void*)0x3000;
    size_t ctx_size = i2c_get_context_size();
    uint8_t ctx_mem[ctx_size];
    i2c_port_t port = i2c_init(ctx_mem, hal_handle, config);

    int res = i2c_master_receive(port, 0x50, NULL, 2);
    TEST_ASSERT_EQUAL(-1, res);
}

void test_i2c_master_transmit_async_should_Fail_OnInvalidArgs(void) {
    I2C_TypeDef regs;
    void *hal_handle = &regs;
    void *config = (void*)0x3000;
    size_t ctx_size = i2c_get_context_size();
    uint8_t ctx_mem[ctx_size];
    i2c_port_t port = i2c_init(ctx_mem, hal_handle, config);

    int res = i2c_master_transmit_async(port, 0x50, NULL, 2, NULL, NULL);
    TEST_ASSERT_EQUAL(-1, res);
    res = i2c_master_transmit_async(port, 0x50, (const uint8_t*)0x1000, 0, NULL, NULL);
    TEST_ASSERT_EQUAL(-1, res);
}

void test_i2c_master_receive_async_should_Fail_OnInvalidArgs(void) {
    I2C_TypeDef regs;
    void *hal_handle = &regs;
    void *config = (void*)0x3000;
    size_t ctx_size = i2c_get_context_size();
    uint8_t ctx_mem[ctx_size];
    i2c_port_t port = i2c_init(ctx_mem, hal_handle, config);

    int res = i2c_master_receive_async(port, 0x50, NULL, 2, NULL, NULL);
    TEST_ASSERT_EQUAL(-1, res);
    res = i2c_master_receive_async(port, 0x50, (uint8_t*)0x1000, 0, NULL, NULL);
    TEST_ASSERT_EQUAL(-1, res);
}

void test_i2c_master_transmit_async_should_CallHalStart(void) {
    I2C_TypeDef regs;
    void *hal_handle = &regs;
    void *config = (void*)0x3000;
    size_t ctx_size = i2c_get_context_size();
    uint8_t ctx_mem[ctx_size];
    i2c_port_t port = i2c_init(ctx_mem, hal_handle, config);
    uint8_t data[2] = {0x01, 0x02};

    mock_i2c_start_return = 0;
    int res = i2c_master_transmit_async(port, 0x50, data, sizeof(data), NULL, NULL);
    TEST_ASSERT_EQUAL(0, res);
    TEST_ASSERT_EQUAL(1, mock_i2c_start_called);
}

void test_i2c_master_receive_async_should_Fail_WhenHalStartFails(void) {
    I2C_TypeDef regs;
    void *hal_handle = &regs;
    void *config = (void*)0x3000;
    size_t ctx_size = i2c_get_context_size();
    uint8_t ctx_mem[ctx_size];
    i2c_port_t port = i2c_init(ctx_mem, hal_handle, config);
    uint8_t data[2];

    mock_i2c_start_return = -1;
    int res = i2c_master_receive_async(port, 0x50, data, sizeof(data), NULL, NULL);
    TEST_ASSERT_EQUAL(-1, res);
    TEST_ASSERT_EQUAL(1, mock_i2c_start_called);
}

void test_i2c_async_callback_should_Report_Ok_OnStop(void) {
    I2C_TypeDef regs;
    void *hal_handle = &regs;
    void *config = (void*)0x3000;
    size_t ctx_size = i2c_get_context_size();
    uint8_t ctx_mem[ctx_size];
    i2c_port_t port = i2c_init(ctx_mem, hal_handle, config);
    uint8_t data[2] = {0x01, 0x02};

    i2c_cb_called = 0;
    i2c_cb_status = I2C_STATUS_ERR;
    mock_i2c_start_return = 0;
    mock_i2c_stop_detected = 1;
    mock_i2c_nack_detected = 0;

    int res = i2c_master_transmit_async(port, 0x50, data, sizeof(data), i2c_test_callback, NULL);
    TEST_ASSERT_EQUAL(0, res);
    i2c_core_ev_irq_handler(port);

    TEST_ASSERT_EQUAL(1, i2c_cb_called);
    TEST_ASSERT_EQUAL(I2C_STATUS_OK, i2c_cb_status);
}

void test_i2c_async_callback_should_Report_Nack_OnError(void) {
    I2C_TypeDef regs;
    void *hal_handle = &regs;
    void *config = (void*)0x3000;
    size_t ctx_size = i2c_get_context_size();
    uint8_t ctx_mem[ctx_size];
    i2c_port_t port = i2c_init(ctx_mem, hal_handle, config);
    uint8_t data[2] = {0x01, 0x02};

    i2c_cb_called = 0;
    i2c_cb_status = I2C_STATUS_OK;
    mock_i2c_start_return = 0;
    mock_i2c_nack_detected = 1;
    mock_i2c_stop_detected = 0;

    int res = i2c_master_transmit_async(port, 0x50, data, sizeof(data), i2c_test_callback, NULL);
    TEST_ASSERT_EQUAL(0, res);
    i2c_core_er_irq_handler(port);

    TEST_ASSERT_EQUAL(1, i2c_cb_called);
    TEST_ASSERT_EQUAL(I2C_STATUS_NACK, i2c_cb_status);
}

void run_i2c_tests(void) {
    printf("\n=== Starting I2C Tests ===\n");

    test_setUp_hook = setUp_local;
    test_tearDown_hook = tearDown_local;
    UnitySetTestFile("tests/test_i2c.c");
    RUN_TEST(test_i2c_create_should_InitHal);
    RUN_TEST(test_i2c_master_transmit_should_CallHal);
    RUN_TEST(test_i2c_master_receive_should_CallHal);
    RUN_TEST(test_i2c_master_transmit_should_Fail_OnNullData);
    RUN_TEST(test_i2c_master_receive_should_Fail_OnNullBuffer);
    RUN_TEST(test_i2c_master_transmit_async_should_Fail_OnInvalidArgs);
    RUN_TEST(test_i2c_master_receive_async_should_Fail_OnInvalidArgs);
    RUN_TEST(test_i2c_master_transmit_async_should_CallHalStart);
    RUN_TEST(test_i2c_master_receive_async_should_Fail_WhenHalStartFails);
    RUN_TEST(test_i2c_async_callback_should_Report_Ok_OnStop);
    RUN_TEST(test_i2c_async_callback_should_Report_Nack_OnError);
    printf("=== I2C Tests Complete ===\n");
}
