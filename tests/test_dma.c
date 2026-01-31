#include "unity.h"
#include "dma.h"
#include "mock_drivers.h"
#include "allocator.h"
#include "test_common.h"
#include <stdio.h>

struct dma_context {
    void *hal_handle;
};

static void setUp_local(void) {
    mock_drivers_reset();
}

static void tearDown_local(void) {
}

void test_dma_create_should_InitHal(void) {
    void* hal_handle = (void*)0x4000;
    void* config = (void*)0x3000;

    dma_channel_t ch = dma_create(hal_handle, config);
    TEST_ASSERT_NOT_NULL(ch);
    TEST_ASSERT_EQUAL_PTR(hal_handle, ch->hal_handle);
    TEST_ASSERT_EQUAL(1, mock_dma_init_called);
}

void test_dma_start_should_CallHalStart(void) {
    struct dma_context ctx;
    ctx.hal_handle = (void*)0x4000;
    
    uintptr_t src = 0x1000;
    uintptr_t dst = 0x2000;
    
    int res = dma_start(&ctx, (void*)src, (void*)dst, 100);
    TEST_ASSERT_EQUAL(0, res);
    TEST_ASSERT_EQUAL(1, mock_dma_start_called);
}

void test_dma_stop_should_CallHalStop(void) {
    struct dma_context ctx;
    ctx.hal_handle = (void*)0x4000;
    
    dma_stop(&ctx);
    TEST_ASSERT_EQUAL(1, mock_dma_stop_called);
}

void run_dma_tests(void) {
    printf("\n=== Starting DMA Tests ===\n");

    test_setUp_hook = setUp_local;
    test_tearDown_hook = tearDown_local;
    UnitySetTestFile("tests/test_dma.c");
    RUN_TEST(test_dma_create_should_InitHal);
    RUN_TEST(test_dma_start_should_CallHalStart);
    RUN_TEST(test_dma_stop_should_CallHalStop);

    printf("=== DMA Tests Complete ===\n");
}