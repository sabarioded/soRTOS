#include "unity.h"
#include "flash.h"
#include "mock_drivers.h"
#include "test_common.h"
#include <stdio.h>

/* Include driver source directly as it is missing from the build configuration */
#include "../drivers/src/flash.c"

static void setUp_local(void) {
    mock_drivers_reset();
}

static void tearDown_local(void) {
}

void test_flash_unlock_should_CallHal(void) {
    flash_unlock();
    TEST_ASSERT_EQUAL(1, mock_flash_unlock_called);
}

void test_flash_lock_should_CallHal(void) {
    flash_lock();
    TEST_ASSERT_EQUAL(1, mock_flash_lock_called);
}

void test_flash_erase_page_should_CallHal(void) {
    mock_flash_erase_return = 0;
    int res = flash_erase_page(0x08001000);
    TEST_ASSERT_EQUAL(0, res);
    TEST_ASSERT_EQUAL_HEX32(0x08001000, mock_flash_erase_addr);

    mock_flash_erase_return = -1;
    res = flash_erase_page(0x08002000);
    TEST_ASSERT_EQUAL(-1, res);
}

void test_flash_program_should_CallHal_WhenAligned(void) {
    uint64_t data = 0x123456789ABCDEF0;
    mock_flash_program_return = 0;
    
    int res = flash_program(0x08001000, &data, 8);
    TEST_ASSERT_EQUAL(0, res);
    TEST_ASSERT_EQUAL_HEX32(0x08001000, mock_flash_program_addr);
    TEST_ASSERT_EQUAL(8, mock_flash_program_len);
}

void test_flash_program_should_Fail_WhenNotAligned(void) {
    uint32_t data = 0x12345678;
    /* Length 4 is not multiple of 8 (STM32L4 requirement) */
    int res = flash_program(0x08001000, &data, 4);
    TEST_ASSERT_EQUAL(-1, res);
    /* Should not have called HAL */
    TEST_ASSERT_EQUAL(0, mock_flash_program_len);
}

void run_flash_tests(void) {
    printf("\n=== Starting Flash Tests ===\n");

    test_setUp_hook = setUp_local;
    test_tearDown_hook = tearDown_local;
    UnitySetTestFile("tests/test_flash.c");
    RUN_TEST(test_flash_unlock_should_CallHal);
    RUN_TEST(test_flash_lock_should_CallHal);
    RUN_TEST(test_flash_erase_page_should_CallHal);
    RUN_TEST(test_flash_program_should_CallHal_WhenAligned);
    RUN_TEST(test_flash_program_should_Fail_WhenNotAligned);

    printf("=== Flash Tests Complete ===\n");
}