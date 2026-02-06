#include "flash.h"
#include "flash_hal.h"
#include "utils.h"

void flash_unlock(void) {
    flash_hal_unlock();
}

void flash_lock(void) {
    flash_hal_lock();
}

int flash_erase_page(uint32_t page_addr) {
    return flash_hal_erase_page(page_addr);
}

int flash_program(uint32_t addr, const void *data, size_t len) {
    if (!data || (len == 0)) {
        return -1;
    }
    /* Platform-specific alignment/granularity checks are handled by the HAL. */
    return flash_hal_program(addr, data, len);
}

int flash_read(uintptr_t addr, void *out, size_t len) {
    if (!out || len == 0U) {
        return -1;
    }
    utils_memcpy(out, (const void *)addr, len);
    return 0;
}
