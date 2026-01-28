#include "unity.h"
#include "mempool.h"
#include "allocator.h"
#include "test_common.h"

static uint8_t heap[4096];

static void setUp_local(void) {
    allocator_init(heap, sizeof(heap));
}

static void tearDown_local(void) {

}

void test_mempool_create_delete(void) {
    mempool_t *pool = mempool_create(32, 10);
    TEST_ASSERT_NOT_NULL(pool);
    mempool_delete(pool);
}

void test_mempool_alloc_free(void) {
    mempool_t *pool = mempool_create(32, 2);
    
    void *p1 = mempool_alloc(pool);
    TEST_ASSERT_NOT_NULL(p1);
    
    void *p2 = mempool_alloc(pool);
    TEST_ASSERT_NOT_NULL(p2);
    TEST_ASSERT_NOT_EQUAL(p1, p2);
    
    /* Pool is full */
    void *p3 = mempool_alloc(pool);
    TEST_ASSERT_NULL(p3);
    
    mempool_free(pool, p1);
    
    /* Should be able to alloc again */
    p3 = mempool_alloc(pool);
    TEST_ASSERT_NOT_NULL(p3);
    /* It typically returns the most recently freed item (LIFO) */
    TEST_ASSERT_EQUAL_PTR(p1, p3);
    
    mempool_delete(pool);
}

void test_mempool_small_item_size(void) {
    /* Request 1 byte items. Should be padded to sizeof(void*) internally */
    mempool_t *pool = mempool_create(1, 10);
    
    void *p = mempool_alloc(pool);
    TEST_ASSERT_NOT_NULL(p);
    
    /* Verify we can write to it without crashing */
    *(uint8_t*)p = 0xAA;
    
    mempool_free(pool, p);
    mempool_delete(pool);
}

void test_mempool_invalid_free(void) {
    mempool_t *pool = mempool_create(32, 5);
    void *p = mempool_alloc(pool);
    
    /* Try to free a pointer outside the pool */
    uint8_t dummy;
    mempool_free(pool, &dummy);
    
    /* Try to free NULL */
    mempool_free(pool, NULL);
    
    /* Clean up valid pointer */
    mempool_free(pool, p);
    mempool_delete(pool);
}

void test_mempool_create_invalid_args(void) {
    TEST_ASSERT_NULL(mempool_create(0, 10));
    TEST_ASSERT_NULL(mempool_create(10, 0));
}

void test_mempool_create_oom(void) {
    /* Try to allocate more than the available heap */
    /* Heap is 4096. Requesting ~10KB */
    mempool_t *pool = mempool_create(1024, 10);
    TEST_ASSERT_NULL(pool);
}

void test_mempool_null_pool_ops(void) {
    TEST_ASSERT_NULL(mempool_alloc(NULL));
    
    /* Should not crash */
    mempool_free(NULL, (void*)0xDEADBEEF);
    mempool_delete(NULL);
}

void test_mempool_free_bounds_check(void) {
    /* Create pool with 2 items of size 32 */
    mempool_t *pool = mempool_create(32, 2);
    
    void *p1 = mempool_alloc(pool);
    void *p2 = mempool_alloc(pool);
    TEST_ASSERT_NOT_NULL(p1);
    TEST_ASSERT_NOT_NULL(p2);
    
    /* Try to free pointers just outside the buffer range */
    uint8_t *start = (uint8_t*)p1; /* p1 should be the start of buffer */
    uint8_t *end = (uint8_t*)p2 + 32; /* p2 is start+32, so p2+32 is end of buffer */
    
    mempool_free(pool, start - 1);
    mempool_free(pool, end);
    
    /* Verify nothing was actually freed (alloc still returns NULL) */
    TEST_ASSERT_NULL(mempool_alloc(pool));
    
    /* Clean up */
    mempool_free(pool, p1);
    mempool_free(pool, p2);
    mempool_delete(pool);
}

void run_mempool_tests(void) {
    printf("\n=== Starting Mempool Tests ===\n");
    
    test_setUp_hook = setUp_local;
    test_tearDown_hook = tearDown_local;
    UnitySetTestFile("tests/test_mempool.c");
    
    RUN_TEST(test_mempool_create_delete);
    RUN_TEST(test_mempool_alloc_free);
    RUN_TEST(test_mempool_small_item_size);
    RUN_TEST(test_mempool_invalid_free);
    RUN_TEST(test_mempool_create_invalid_args);
    RUN_TEST(test_mempool_create_oom);
    RUN_TEST(test_mempool_null_pool_ops);
    RUN_TEST(test_mempool_free_bounds_check);
    
    printf("=== Mempool Tests Complete ===\n");
}
