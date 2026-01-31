#include "flash.h"
#include "flash_hal.h"

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
    /* STM32L4 requires 64-bit aligned address and length. */
    if ((addr & 0x7U) != 0U || (len & 0x7U) != 0U) {
        return -1;
    }
    return flash_hal_program(addr, data, len);
}
