#include "unity.h"
#include "allocator.h"
#include <string.h>
#include <stddef.h>
#include <stdio.h>
#include "test_common.h"

typedef struct BlockHeader {
    size_t size; 
    struct BlockHeader *prev_phys_block;
    struct BlockHeader *next_free;
    struct BlockHeader *prev_free;
} block_header_t;

#define BLOCK_OVERHEAD      (offsetof(block_header_t, next_free))
#define BLOCK_MIN_SIZE      (sizeof(block_header_t))
#define ALIGN_SIZE          8
#define ALIGN(size)         (((size) + (ALIGN_SIZE - 1)) & ~(ALIGN_SIZE - 1))

#define GET_SIZE(x)         ((x)->size & ~3)

#define POOL_SIZE 4096

static uint8_t test_pool[POOL_SIZE];

/* Initialize the allocator with the test pool */
static void setUp_local(void) {
    /* Reset the allocator before every test */
    allocator_init(test_pool, POOL_SIZE);
}

/* Teardown verify heap integrity */
static void tearDown_local(void) { 
    TEST_ASSERT_EQUAL(0, allocator_check_integrity());
}

/* Verify that malloc returns a valid pointer for a normal request */
void test_should_return_pointer_when_requesting_memory(void) {
    void* ptr = allocator_malloc(128);
    TEST_ASSERT_NOT_NULL(ptr);
}

/* Verify that malloc returns NULL when the pool is full */
void test_should_return_null_when_pool_is_exhausted(void) {
    void* ptr = allocator_malloc(POOL_SIZE + 1);
    TEST_ASSERT_NULL(ptr);
}

/* Verify that multiple allocations return distinct addresses */
void test_should_allow_multiple_allocations(void) {
    void* ptr1 = allocator_malloc(64);
    void* ptr2 = allocator_malloc(64);
    
    TEST_ASSERT_NOT_NULL(ptr1);
    TEST_ASSERT_NOT_NULL(ptr2);
    TEST_ASSERT_NOT_EQUAL(ptr1, ptr2); /* Ensure they are different addresses */
}

/* Verify that freeing memory makes it available for allocation again */
void test_should_reclaim_memory_after_free(void) {
    void* ptr1 = allocator_malloc(POOL_SIZE - 64); /* Leave a tiny bit for headers */
    TEST_ASSERT_NOT_NULL(ptr1);
    
    void* ptr2 = allocator_malloc(POOL_SIZE / 2);
    TEST_ASSERT_NULL(ptr2);
    
    allocator_free(ptr1);
    void* ptr3 = allocator_malloc(POOL_SIZE / 2);
    TEST_ASSERT_NOT_NULL(ptr3);
}

/* Verify that adjacent free blocks are merged (coalesced) */
void test_should_merge_adjacent_blocks(void) {
    size_t half_size = (POOL_SIZE / 2) - 64;
    void* p1 = allocator_malloc(half_size);
    void* p2 = allocator_malloc(half_size);
    TEST_ASSERT_NOT_NULL(p1);
    TEST_ASSERT_NOT_NULL(p2);
    
    allocator_free(p1);
    allocator_free(p2);
    
    void* p3 = allocator_malloc(half_size + 100);
    TEST_ASSERT_NOT_NULL(p3);
}

/* Verify that allocations respect the platform alignment requirements */
void test_should_maintain_alignment(void) {
    /* Request an "ugly" size like 5 bytes */
    void* p1 = allocator_malloc(5);
    
    /* The address must be a multiple of the system's alignment (usually 4 or 8) */
    uintptr_t addr = (uintptr_t)p1;
    TEST_ASSERT_EQUAL_UINT32(0, addr % sizeof(void*));
}

/* Verify that requesting a negative size (huge unsigned) returns NULL */
void test_should_return_null_negative_malloc(void) {
    void* p1 = allocator_malloc(-5);

    TEST_ASSERT_NULL(p1);
}

/* Verify that blocks are not split if the remainder is too small */
void test_should_not_split_if_remaining_space_is_too_small(void) {
    /* 
     * Request a size that leaves exactly 4 bytes leftover 
     * (not enough for a new header + data)
     * POOL_SIZE is 4096. Overhead is 16. Total usable 4080.
     * We want to leave < 16 bytes.
     * malloc(4064) -> 4064+16 = 4080. Remaining 0.
     */
    void* p1 = allocator_malloc(4064);

    TEST_ASSERT_NOT_NULL(p1);
    
    /* The next malloc should fail because no split occurred */
    void* p2 = allocator_malloc(1);
    TEST_ASSERT_NULL(p2);
}

/* Verify that freeing a block between two free blocks merges all three */
void test_should_merge_three_blocks_into_one(void) {
    void* p1 = allocator_malloc(64);
    void* p2 = allocator_malloc(64);
    void* p3 = allocator_malloc(64);
    
    allocator_free(p1);
    allocator_free(p3);
    /* Middle block is still taken. Now free it. */
    allocator_free(p2);
    
    /* Now we should be able to allocate the sum of all three + their headers */
    void* p_big = allocator_malloc(192);
    TEST_ASSERT_NOT_NULL(p_big);
}

/* Verify that the allocator skips small holes to find a block that fits */
void test_should_skip_small_holes_to_find_fitting_block(void) {
    void* p1 = allocator_malloc(32); /* Hole 1 */
    void* p_gap = allocator_malloc(32); /* Filler */
    void* p2 = allocator_malloc(128); /* Hole 2 */
    void* p_gap2 = allocator_malloc(32); /* Filler */
    
    TEST_ASSERT_NOT_NULL(p1);
    TEST_ASSERT_NOT_NULL(p_gap);
    TEST_ASSERT_NOT_NULL(p2);
    TEST_ASSERT_NOT_NULL(p_gap2);

    allocator_free(p1);
    allocator_free(p2);
    
    /* 100 bytes won't fit in Hole 1 (32), must find Hole 2 (128) */
    void* p3 = allocator_malloc(100);
    TEST_ASSERT_NOT_NULL(p3);
    /* Verify it actually used the second hole's address */
    TEST_ASSERT_EQUAL_PTR(p2, p3); 
}

/* Verify that the free size statistic is accurate */
void test_should_accurately_report_free_space(void) {
    size_t initial_free = allocator_get_free_size();
    void* p1 = allocator_malloc(100);
    
    size_t after_malloc = allocator_get_free_size();
    /* It should be less by at least 100 */
    TEST_ASSERT_TRUE(after_malloc < initial_free);
    
    allocator_free(p1);
    TEST_ASSERT_EQUAL_INT(initial_free, allocator_get_free_size());
}

/* Verify that freeing everything restores the full pool size */
void test_free_mem_should_return_to_original_after_full_cycle(void) {
    size_t initial_free = allocator_get_free_size();
    
    void* p1 = allocator_malloc(100);
    void* p2 = allocator_malloc(200);
    void* p3 = allocator_malloc(50);
    
    allocator_free(p1);
    allocator_free(p2);
    allocator_free(p3);
    
    /* If your math is correct, this must be EXACTLY initial_free */
    TEST_ASSERT_EQUAL_UINT32(initial_free, allocator_get_free_size());
}

/* Verify that realloc preserves data when growing in place (or moving) */
void test_realloc_should_preserve_data(void) {
    char* data = allocator_malloc(10);
    strcpy(data, "STM32");
    
    /* Force a move by requesting something much larger */
    data = allocator_realloc(data, 500);
    
    TEST_ASSERT_NOT_NULL(data);
    TEST_ASSERT_EQUAL_STRING("STM32", data);
    allocator_free(data);
}

/* Verify that realloc preserves data when moving to a new location */
void test_realloc_should_preserve_data_after_move(void) {
    char* original = allocator_malloc(16);
    strcpy(original, "TestContent");
    
    /* Force a move by allocating a "blocker" right after it */
    void* blocker = allocator_malloc(64);
    
    /* Now grow 'original' - it must move because 'blocker' is in the way */
    char* resized = allocator_realloc(original, 128);
    
    TEST_ASSERT_NOT_NULL(resized);
    TEST_ASSERT_EQUAL_STRING("TestContent", resized);
    TEST_ASSERT_NOT_EQUAL(original, resized); /* Verify it actually moved */
    
    allocator_free(blocker);
    allocator_free(resized);
}

/* Verify that shrinking a block returns the excess memory to the pool */
void test_realloc_shrinking_should_reclaim_space(void) {
    size_t initial_free = allocator_get_free_size();
    void* p1 = allocator_malloc(500);
    
    /* Shrink 500 down to 100 */
    p1 = allocator_realloc(p1, 100);
    
    /* We should have reclaimed roughly 400 bytes (minus the new header overhead) */
    size_t after_shrink = allocator_get_free_size();
    TEST_ASSERT_TRUE(after_shrink > (initial_free - 500));
    
    allocator_free(p1);
}

/* Verify allocation behavior when the heap is exactly full */
void test_exhaustion_at_boundary(void) {
    size_t remaining = allocator_get_free_size();
    
    /* Try to take everything */
    void* p1 = allocator_malloc(remaining);
    TEST_ASSERT_NOT_NULL(p1);
    TEST_ASSERT_EQUAL_UINT32(0, allocator_get_free_size());
    
    /* Should fail now */
    void* p2 = allocator_malloc(1);
    TEST_ASSERT_NULL(p2);
    
    allocator_free(p1);
}

/* Verify that the fragment count statistic is accurate */
void test_fragment_count_accuracy(void) {
    void* p1 = allocator_malloc(32);
    void* p2 = allocator_malloc(32);
    void* p3 = allocator_malloc(32);
    
    /* Free the middle block to create a "hole" */
    allocator_free(p2);
    /* Fragment count should be 2 (The hole in the middle + the large remainder at the end) */
    TEST_ASSERT_EQUAL_INT(2, allocator_get_fragment_count());
    
    allocator_free(p1);
    allocator_free(p3);
    /* After coalescing, fragments should go back to 1 */
    TEST_ASSERT_EQUAL_INT(1, allocator_get_fragment_count());
}

/* Verify that integrity check detects a corrupted block header size */
void test_integrity_check_should_detect_header_corruption(void) {
    void* p1 = allocator_malloc(64);
    
    /* Get pointer to the header */
    block_header_t* header = (block_header_t*)((uint8_t*)p1 - BLOCK_OVERHEAD);
    
    /* Corrupt the size (make it unaligned) */
    size_t original_size = header->size;
    header->size |= 0x2; /* Set bit 1, which should always be 0 for 8-byte alignment */
    
    /* Expect error -2 (Block Alignment Error) */
    /* Note: We cast to int to match return type */
    TEST_ASSERT_EQUAL(-2, allocator_check_integrity());
    
    /* Restore to pass tearDown */
    header->size = original_size;
}

/* Verify that integrity check detects a block size extending past heap end */
void test_integrity_check_should_detect_boundary_overflow(void) {
    void* p1 = allocator_malloc(64);
    block_header_t* header = (block_header_t*)((uint8_t*)p1 - BLOCK_OVERHEAD);
    
    size_t original_size = header->size;
    
    /* Set size to something massive that goes beyond POOL_SIZE */
    header->size = (POOL_SIZE * 2) | (original_size & 1);
    
    /* Expect error -4 (Size pushes past heap end) */
    TEST_ASSERT_EQUAL(-4, allocator_check_integrity());
    
    header->size = original_size;
}

/* Verify that integrity check detects a broken physical block chain */
void test_integrity_check_should_detect_broken_physical_chain(void) {
    void* p1 = allocator_malloc(64);
    void* p2 = allocator_malloc(64);
    
    block_header_t* h2 = (block_header_t*)((uint8_t*)p2 - BLOCK_OVERHEAD);
    block_header_t* original_prev = h2->prev_phys_block;
    
    /* Break the chain */
    h2->prev_phys_block = NULL;
    
    TEST_ASSERT_EQUAL(-5, allocator_check_integrity());
    
    h2->prev_phys_block = original_prev;
    allocator_free(p1);
    allocator_free(p2);
}

/* Run the allocator test suite */
void run_allocator_tests(void) {
    test_setUp_hook = setUp_local;
    test_tearDown_hook = tearDown_local;
    UnitySetTestFile("tests/test_allocator.c");
    printf("\n=== Starting Allocator Tests ===\n");
    RUN_TEST(test_should_return_pointer_when_requesting_memory);
    RUN_TEST(test_should_return_null_when_pool_is_exhausted);
    RUN_TEST(test_should_allow_multiple_allocations);    
    RUN_TEST(test_should_reclaim_memory_after_free);
    RUN_TEST(test_should_merge_adjacent_blocks);
    RUN_TEST(test_should_maintain_alignment);
    RUN_TEST(test_should_return_null_negative_malloc);
    RUN_TEST(test_should_not_split_if_remaining_space_is_too_small);
    RUN_TEST(test_should_merge_three_blocks_into_one);
    RUN_TEST(test_should_skip_small_holes_to_find_fitting_block);
    RUN_TEST(test_should_accurately_report_free_space);
    RUN_TEST(test_free_mem_should_return_to_original_after_full_cycle);
    RUN_TEST(test_realloc_should_preserve_data);
    RUN_TEST(test_realloc_should_preserve_data_after_move);
    RUN_TEST(test_realloc_shrinking_should_reclaim_space);
    RUN_TEST(test_exhaustion_at_boundary);
    RUN_TEST(test_fragment_count_accuracy);
    RUN_TEST(test_integrity_check_should_detect_header_corruption);
    RUN_TEST(test_integrity_check_should_detect_boundary_overflow);
    RUN_TEST(test_integrity_check_should_detect_broken_physical_chain);
    printf("=== Allocator Tests Complete ===\n");
}