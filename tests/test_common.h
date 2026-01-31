#ifndef TEST_COMMON_H
#define TEST_COMMON_H

#include <stddef.h>
#include <setjmp.h>

/**
 * @brief Mock system tick counter.
 * Modified by tests to simulate time passing.
 */
extern size_t mock_ticks;

/**
 * @brief Mock CPU frequency.
 * Used by platform_get_cpu_freq().
 */
extern size_t mock_cpu_freq;

/**
 * @brief Counter for how many times platform_yield() was called.
 */
extern int mock_yield_count;

/**
 * @brief Jump buffer for simulating context switches.
 * Used by setjmp/longjmp in tests to catch yield calls.
 */
extern jmp_buf yield_jump;

/**
 * @brief Hook for test setup.
 * Assigned by individual test files to run their specific setup logic.
 */
extern void (*test_setUp_hook)(void);

/**
 * @brief Hook for test teardown.
 * Assigned by individual test files to run their specific teardown logic.
 */
extern void (*test_tearDown_hook)(void);

#endif /* TEST_COMMON_H */