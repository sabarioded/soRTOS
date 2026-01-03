#ifndef SYSTEM_CLOCK_H
#define SYSTEM_CLOCK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SYSTEM_CLOCK_OK                 0
#define SYSTEM_CLOCK_ERR_UNSUPPORTED    (-1)
#define SYSTEM_CLOCK_ERR_TIMEOUT        (-2)

/* Default busy-wait loop iteration limit used when waiting for
 * hardware flags (MSI/HSI/PLL ready, clock switch, etc.)
 */
#define SYSTEM_CLOCK_WAIT_MAX_ITER      (1000000U)

/* Supported system clock frequencies (Hz) */
typedef enum {
    SYSCLOCK_HZ_4MHZ  = 4000000UL,
    SYSCLOCK_HZ_16MHZ = 16000000UL,
    SYSCLOCK_HZ_24MHZ = 24000000UL,
    SYSCLOCK_HZ_32MHZ = 32000000UL,
    SYSCLOCK_HZ_48MHZ = 48000000UL,
    SYSCLOCK_HZ_64MHZ = 64000000UL,
    SYSCLOCK_HZ_80MHZ = 80000000UL,
} sysclock_hz_t;

/**
 * @brief Configure system clock (SYSCLK) to X Hz.
 *
 * Supported values: 4, 16, 24, 32, 48, 64, 80 MHz.
 *
 * @param target_hz  Desired SYSCLK frequency in Hz.
 * @return `SYSTEM_CLOCK_OK` (0) on success,
 *         `SYSTEM_CLOCK_ERR_UNSUPPORTED` (-1) if `target_hz` is not supported,
 *         `SYSTEM_CLOCK_ERR_TIMEOUT` (-2) if a hardware ready/switch wait timed out.
 */
int system_clock_config_hz(sysclock_hz_t target_hz);

/**
 * @brief Get current System Clock in Hz.
 *
 * This returns the last value successfully configured by system_clock_config_hz().
 */
uint32_t get_system_clock_hz(void);

#ifdef __cplusplus
}
#endif

#endif /* SYSTEM_CLOCK_H */
