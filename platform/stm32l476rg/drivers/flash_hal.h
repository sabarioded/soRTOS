#ifndef FLASH_HAL_STM32_H
#define FLASH_HAL_STM32_H

#include "device_registers.h"
#include "arch_ops.h"

/* Flash Keys */
#define FLASH_KEY1              0x45670123U
#define FLASH_KEY2              0xCDEF89ABU

/* Flash Control Register (CR) Bits */
#define FLASH_CR_PG             (1U << 0)
#define FLASH_CR_PER            (1U << 1)
#define FLASH_CR_STRT           (1U << 16)
#define FLASH_CR_LOCK           (1U << 31)
#define FLASH_CR_PNB_Pos        (3U)
#define FLASH_CR_PNB_Msk        (0xFFU << FLASH_CR_PNB_Pos)

/* Flash Status Register (SR) Bits */
#define FLASH_SR_BSY            (1U << 16)
#define FLASH_SR_EOP            (1U << 0)
#define FLASH_SR_PGSERR         (1U << 7)

/**
 * @brief Wait for Flash operation to complete.
 */
static inline void flash_hal_wait_busy(void) {
    while (FLASH->SR & FLASH_SR_BSY) {
        arch_nop();
    }
}

/**
 * @brief Unlock Flash.
 */
static inline void flash_hal_unlock(void) {
    if (FLASH->CR & FLASH_CR_LOCK) {
        FLASH->KEYR = FLASH_KEY1;
        FLASH->KEYR = FLASH_KEY2;
    }
}

/**
 * @brief Lock Flash.
 */
static inline void flash_hal_lock(void) {
    FLASH->CR |= FLASH_CR_LOCK;
}

/**
 * @brief Erase a page.
 */
static inline int flash_hal_erase_page(uint32_t page_addr) {
    flash_hal_wait_busy();

    /* Clear error flags */
    FLASH->SR = (FLASH_SR_EOP | FLASH_SR_PGSERR);

    /* Calculate Page Number (2KB pages on STM32L476) */
    /* Base is 0x08000000 */
    uint32_t page = (page_addr - 0x08000000) / 2048;

    /* Configure Erase */
    uint32_t cr = FLASH->CR;
    cr &= ~FLASH_CR_PNB_Msk;
    cr |= (page << FLASH_CR_PNB_Pos);
    cr |= FLASH_CR_PER;
    FLASH->CR = cr;

    /* Start Erase */
    FLASH->CR |= FLASH_CR_STRT;

    flash_hal_wait_busy();

    /* Disable PER */
    FLASH->CR &= ~FLASH_CR_PER;

    return (FLASH->SR & FLASH_SR_PGSERR) ? -1 : 0;
}

/**
 * @brief Program data (Double Word - 64-bit).
 */
static inline int flash_hal_program(uint32_t addr, const void *data, size_t len) {
    if (!data || len == 0U) {
        return -1;
    }
    /* STM32L4 requires 64-bit aligned address and length. */
    if ((addr & 0x7U) != 0U || (len & 0x7U) != 0U) {
        return -1;
    }

    const uint64_t *src = (const uint64_t *)data;
    volatile uint64_t *dst = (volatile uint64_t *)addr;
    size_t count = len / 8;

    flash_hal_wait_busy();
    FLASH->SR = (FLASH_SR_EOP | FLASH_SR_PGSERR);

    /* Enable Programming */
    FLASH->CR |= FLASH_CR_PG;

    for (size_t i = 0; i < count; i++) {
        dst[i] = src[i];
        flash_hal_wait_busy();
        if (FLASH->SR & FLASH_SR_PGSERR) {
            FLASH->CR &= ~FLASH_CR_PG;
            return -1;
        }
    }

    FLASH->CR &= ~FLASH_CR_PG;
    return 0;
}

#endif /* FLASH_HAL_STM32_H */
