#ifndef FLASH_HAL_NATIVE_H
#define FLASH_HAL_NATIVE_H

#include <stdint.h>
#include <stddef.h>

void flash_hal_unlock(void);
void flash_hal_lock(void);
int flash_hal_erase_page(uint32_t page_addr);
int flash_hal_program(uint32_t addr, const void *data, size_t len);

#endif /* FLASH_HAL_NATIVE_H */
