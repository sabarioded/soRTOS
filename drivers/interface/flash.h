#ifndef FLASH_H
#define FLASH_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Unlock the Flash memory for writing/erasing.
 */
void flash_unlock(void);

/**
 * @brief Lock the Flash memory to prevent accidental writes.
 */
void flash_lock(void);

/**
 * @brief Erase a page of Flash memory.
 * @param page_addr The start address of the page to erase.
 *        Page size and address alignment are platform-specific.
 * @return int 0 on success, -1 on error.
 */
int flash_erase_page(uint32_t page_addr);

/**
 * @brief Program data into Flash memory.
 * Note: The destination must be erased before programming.
 *       Alignment and program granularity are platform-specific and enforced
 *       by the HAL. If requirements are not met, this returns -1.
 * 
 * @param addr The address to start writing to.
 * @param data Pointer to the data buffer.
 * @param len Number of bytes to write.
 * @return int 0 on success, -1 on error.
 */
int flash_program(uint32_t addr, const void *data, size_t len);

/**
 * @brief Read data from Flash memory.
 * Note: Flash is typically memory-mapped; this performs a simple copy.
 *       No bounds checking is performed in the generic driver.
 *
 * @param addr The address to start reading from.
 * @param out Pointer to the destination buffer.
 * @param len Number of bytes to read.
 * @return int 0 on success, -1 on error.
 */
int flash_read(uintptr_t addr, void *out, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* FLASH_H */
