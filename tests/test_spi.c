#include "unity.h"
#include "spi.h"
#include "mock_drivers.h"
#include "allocator.h"
#include "test_common.h"
#include <stdio.h>

/* Dummy definition for the opaque struct to access the first field in tests. */
struct spi_context {
    void *hal_handle;
};

static void setUp_local(void) {
    mock_drivers_reset();
}

static void tearDown_local(void) {
}

void test_spi_create_should_AllocateAndInit(void) {
    void* hal_handle = (void*)0xDEADBEEF;
    void* config = (void*)0xCAFEBABE;

    spi_port_t port = spi_create(hal_handle, config);
    TEST_ASSERT_NOT_NULL(port);
    TEST_ASSERT_EQUAL_PTR(hal_handle, port->hal_handle);
}

void test_spi_transfer_should_TransferBytes(void) {
    void *hal_handle = (void*)0x1234;
    void *config = (void*)0xCAFEBABE;
    size_t ctx_size = spi_get_context_size();
    uint8_t ctx_mem[ctx_size];
    spi_port_t port = spi_init(ctx_mem, hal_handle, config);
    
    uint8_t tx_data[] = {0xAA, 0xBB};
    uint8_t rx_data[2];

    mock_spi_transfer_return = 0x11; /* Simple mock returns same byte for all calls */
    
    int res = spi_transfer(port, tx_data, rx_data, 2);
    TEST_ASSERT_EQUAL(0, res);
    TEST_ASSERT_EQUAL_HEX8(0x11, rx_data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x11, rx_data[1]);
}

void run_spi_tests(void) {
    printf("\n=== Starting SPI Tests ===\n");

    test_setUp_hook = setUp_local;
    test_tearDown_hook = tearDown_local;
    UnitySetTestFile("tests/test_spi.c");
    RUN_TEST(test_spi_create_should_AllocateAndInit);
    RUN_TEST(test_spi_transfer_should_TransferBytes);
    printf("=== SPI Tests Complete ===\n");
}
