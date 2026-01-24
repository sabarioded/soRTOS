#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef NULL
#define NULL ((void *)0)
#endif

/**
 * @brief Wait for specific bits in a register to be SET.
 * 
 * Polls the register until the bits specified by the mask are all set (1).
 * 
 * @param reg Pointer to the volatile register to poll.
 * @param mask Bitmask of bits to check.
 * @param max_iter Maximum number of loop iterations before timing out.
 * @return 0 on success, -1 on timeout.
 */
int wait_for_flag_set(volatile uint32_t *reg, uint32_t mask, uint32_t max_iter);

/**
 * @brief Wait for specific bits in a register to be CLEARED.
 * 
 * Polls the register until the bits specified by the mask are all cleared (0).
 * 
 * @param reg Pointer to the volatile register to poll.
 * @param mask Bitmask of bits to check.
 * @param max_iter Maximum number of loop iterations before timing out.
 * @return 0 on success, -1 on timeout.
 */
int wait_for_flag_clear(volatile uint32_t *reg, uint32_t mask, uint32_t max_iter);

/**
 * @brief Wait for masked bits in a register to equal an expected value.
 * 
 * Polls the register, applies the mask, and compares the result to the expected value.
 * 
 * @param reg Pointer to the volatile register to poll.
 * @param mask Bitmask to apply to the register value.
 * @param expected Expected value after masking.
 * @param max_iter Maximum number of loop iterations before timing out.
 * @return 0 on success, -1 on timeout.
 */
int wait_for_reg_mask_eq(volatile uint32_t *reg, uint32_t mask, uint32_t expected, uint32_t max_iter);

/**
 * @brief Convert a string to an integer.
 * 
 * Parses a string representing a non-negative integer.
 * Stops parsing at the first non-digit character.
 * 
 * @param string Null-terminated string to parse.
 * @return The converted integer value.
 */
int utils_atoi(const char *string);

/**
 * @brief Fill a block of memory with a specific value.
 * 
 * @param s Pointer to the memory block.
 * @param c Value to set (converted to unsigned char).
 * @param n Number of bytes to set.
 * @return Pointer to the memory block (s).
 */
void *utils_memset(void *s, int c, size_t n);

/**
 * @brief Copy a block of memory.
 * 
 * Copies n bytes from src to dest. The memory areas must not overlap.
 * 
 * @param dest Pointer to the destination memory block.
 * @param src Pointer to the source memory block.
 * @param n Number of bytes to copy.
 * @return Pointer to the destination memory block (dest).
 */
void *utils_memcpy(void *dest, const void *src, size_t n);

/**
 * @brief Compare two strings.
 * 
 * Compares the two strings s1 and s2.
 * 
 * @param s1 First string.
 * @param s2 Second string.
 * @return An integer less than, equal to, or greater than zero if s1 is found,
 *         respectively, to be less than, to match, or be greater than s2.
 */
int utils_strcmp(const char *s1, const char *s2);

#ifdef __cplusplus
}
#endif

#endif /* UTILS_H */