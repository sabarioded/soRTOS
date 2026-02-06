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
 * @param reg_addr Pointer to the volatile register to poll.
 * @param bit_mask Bitmask of bits to check.
 * @param timeout_cycles Maximum number of loop iterations before timing out.
 * @return 0 on success, -1 on timeout.
 */
int wait_for_flag_set(volatile uint32_t *reg_addr, uint32_t bit_mask, uint32_t timeout_cycles);

/**
 * @brief Wait for specific bits in a register to be CLEARED.
 * 
 * @param reg_addr Pointer to the volatile register to poll.
 * @param bit_mask Bitmask of bits to check.
 * @param timeout_cycles Maximum number of loop iterations before timing out.
 * @return 0 on success, -1 on timeout.
 */
int wait_for_flag_clear(volatile uint32_t *reg_addr, uint32_t bit_mask, uint32_t timeout_cycles);

/**
 * @brief Wait for masked bits in a register to equal an expected value.
 * 
 * Polls the register, applies the mask, and compares the result to the expected value.
 * 
 * @param reg_addr Pointer to the volatile register to poll.
 * @param bit_mask Bitmask to apply to the register value.
 * @param expected_value Expected value after masking.
 * @param timeout_cycles Maximum number of loop iterations before timing out.
 * @return 0 on success, -1 on timeout.
 */
int wait_for_reg_mask_eq(volatile uint32_t *reg_addr, uint32_t bit_mask, uint32_t expected_value, uint32_t timeout_cycles);

/**
 * @brief Convert a string to an integer.
 * 
 * Parses a string representing a non-negative integer.
 * Stops parsing at the first non-digit character.
 * 
 * @param str Null-terminated string to parse.
 * @return The converted integer value.
 */
int utils_atoi(const char *str);

/**
 * @brief Fill a block of memory with a specific value.
 * 
 * @param dest Pointer to the memory block.
 * @param value Value to set (converted to unsigned char).
 * @param count Number of bytes to set.
 * @return Pointer to the memory block (dest).
 */
void *utils_memset(void *dest, int value, size_t count);

/**
 * @brief Copy a block of memory.
 * 
 * Copies n bytes from src to dest. The memory areas must not overlap.
 * 
 * @param dest Pointer to the destination memory block.
 * @param src Pointer to the source memory block.
 * @param count Number of bytes to copy.
 * @return Pointer to the destination memory block (dest).
 */
void *utils_memcpy(void *dest, const void *src, size_t count);

/**
 * @brief Compare two strings.
 * 
 * Compares the two strings s1 and s2.
 * 
 * @param str1 First string.
 * @param str2 Second string.
 * @return An integer less than, equal to, or greater than zero if s1 is found,
 *         respectively, to be less than, to match, or be greater than s2.
 */
int utils_strcmp(const char *str1, const char *str2);

/**
 * @brief Get the length of a string.
 * 
 * @param str Null-terminated string.
 * @return Number of characters before the null terminator.
 */
size_t utils_strlen(const char *str);

#ifdef __cplusplus
}
#endif

#endif /* UTILS_H */
