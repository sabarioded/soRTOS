#include "unity.h"
#include "utils.h"
#include <string.h>
#include <stdio.h>
#include "test_common.h"

static void setUp_local(void) {
}

static void tearDown_local(void) {
}

/* Verify utils_memset fills memory correctly */
void test_utils_memset(void) {
    uint8_t buffer[10];
    /* Fill with 0xAA */
    utils_memset(buffer, 0xAA, sizeof(buffer));
    
    for (int i = 0; i < 10; i++) {
        TEST_ASSERT_EQUAL_HEX8(0xAA, buffer[i]);
    }
}

/* Verify utils_memcpy copies data correctly */
void test_utils_memcpy(void) {
    char src[] = "Hello";
    char dest[10];
    
    utils_memset(dest, 0, sizeof(dest));
    utils_memcpy(dest, src, 6); /* Copy including null terminator */
    
    TEST_ASSERT_EQUAL_STRING("Hello", dest);
}

/* Verify utils_strcmp behavior */
void test_utils_strcmp(void) {
    TEST_ASSERT_EQUAL(0, utils_strcmp("abc", "abc"));
    TEST_ASSERT_LESS_THAN(0, utils_strcmp("abc", "abd"));
    TEST_ASSERT_GREATER_THAN(0, utils_strcmp("abd", "abc"));
    TEST_ASSERT_LESS_THAN(0, utils_strcmp("abc", "abcd")); /* Length check */
}

/* Verify utils_strlen behavior */
void test_utils_strlen(void) {
    TEST_ASSERT_EQUAL_UINT32(0, utils_strlen(""));
    TEST_ASSERT_EQUAL_UINT32(3, utils_strlen("abc"));
    TEST_ASSERT_EQUAL_UINT32(5, utils_strlen("hello"));
    TEST_ASSERT_EQUAL_UINT32(0, utils_strlen(NULL));
}

/* Verify utils_atoi conversion */
void test_utils_atoi(void) {
    TEST_ASSERT_EQUAL(123, utils_atoi("123"));
    TEST_ASSERT_EQUAL(0, utils_atoi("0"));
    TEST_ASSERT_EQUAL(123, utils_atoi("123a")); /* Stops at non-digit */
    TEST_ASSERT_EQUAL(0, utils_atoi("abc"));    /* Invalid start */
}

/* Verify wait_for_flag_set returns 0 when flag is set */
void test_wait_for_flag_set_success(void) {
    volatile uint32_t reg = 0x00000001;
    /* Wait for bit 0 to be set */
    int res = wait_for_flag_set(&reg, 0x1, 10);
    TEST_ASSERT_EQUAL(0, res);
}

/* Verify wait_for_flag_set returns -1 on timeout */
void test_wait_for_flag_set_timeout(void) {
    volatile uint32_t reg = 0x00000000;
    /* Wait for bit 0 to be set (it never is) */
    int res = wait_for_flag_set(&reg, 0x1, 10);
    TEST_ASSERT_EQUAL(-1, res);
}

/* Verify wait_for_flag_clear returns 0 when flag is clear */
void test_wait_for_flag_clear_success(void) {
    volatile uint32_t reg = 0x00000000;
    /* Wait for bit 0 to be clear */
    int res = wait_for_flag_clear(&reg, 0x1, 10);
    TEST_ASSERT_EQUAL(0, res);
}

/* Verify wait_for_reg_mask_eq returns 0 when match */
void test_wait_for_reg_mask_eq_success(void) {
    volatile uint32_t reg = 0x000000F0;
    /* Mask 0xF0, expect 0xF0 */
    int res = wait_for_reg_mask_eq(&reg, 0xF0, 0xF0, 10);
    TEST_ASSERT_EQUAL(0, res);
}

void run_utils_tests(void) {
    printf("\n=== Starting Utils Tests ===\n");
    test_setUp_hook = setUp_local;
    test_tearDown_hook = tearDown_local;
    UnitySetTestFile("tests/test_utils.c");
    
    RUN_TEST(test_utils_memset);
    RUN_TEST(test_utils_memcpy);
    RUN_TEST(test_utils_strcmp);
    RUN_TEST(test_utils_strlen);
    RUN_TEST(test_utils_atoi);
    RUN_TEST(test_wait_for_flag_set_success);
    RUN_TEST(test_wait_for_flag_set_timeout);
    RUN_TEST(test_wait_for_flag_clear_success);
    RUN_TEST(test_wait_for_reg_mask_eq_success);
    
    printf("=== Utils Tests Complete ===\n");
}
