#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NULL ((void *)0)

/** @brief Wait for specific bits in a register to be SET. Returns 0 on success. */
int wait_for_flag_set(volatile uint32_t *reg, uint32_t mask, uint32_t max_iter);

/** @brief Wait for specific bits in a register to be CLEARED. Returns 0 on success. */
int wait_for_flag_clear(volatile uint32_t *reg, uint32_t mask, uint32_t max_iter);

/** @brief Wait for masked bits to equal expected value. Returns 0 on success. */
int wait_for_reg_mask_eq(volatile uint32_t *reg, uint32_t mask, uint32_t expected, uint32_t max_iter);

/** @brief Convert string to integer. */
int atoi(const char *string);

/** @brief Fill memory with a constant byte. */
void *memset(void *s, int c, size_t n);

/** @brief Copy memory area. */
void *memcpy(void *dest, const void *src, size_t n);

/** @brief Compare two strings. */
int strcmp(const char *s1, const char *s2);

#ifdef __cplusplus
}
#endif

#endif /* UTILS_H */