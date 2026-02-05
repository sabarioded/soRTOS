#ifndef RTC_HAL
#define RTC_HAL

#include "device_registers.h"
#include "rtc.h"
#include "arch_ops.h"

/* RTC time structure (hours/minutes/seconds, 24-hour format) */
struct rtc_time {
    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;
};

/* RTC date structure (day/month/year/weekday) */
struct rtc_date {
    uint8_t day;
    uint8_t month;
    uint8_t year; /* 0-99 */
    uint8_t weekday; /* 1=Mon, 7=Sun */
};

/* RCC & PWR control bits */
#define RCC_APB1ENR1_PWREN      (1U << 28)
#define PWR_CR1_DBP             (1U << 8)
#define RCC_BDCR_LSEON          (1U << 0)
#define RCC_BDCR_LSION          (1U << 0) /* Using LSI for simplicity */
#define RCC_BDCR_RTCEN          (1U << 15)
#define RCC_BDCR_RTCSEL_LSI     (2U << 8)

/* RTC write-protection keys */
#define RTC_WRITE_PROTECTION_KEY1   0xCA
#define RTC_WRITE_PROTECTION_KEY2   0x53

/* RTC ISR bits */
#define RTC_ISR_INIT            (1U << 7)
#define RTC_ISR_INITF           (1U << 6)
#define RTC_ISR_RSF             (1U << 5)

/* Helpers for BCD conversion */
static inline uint8_t rtc_bcd2bin(uint8_t bcd) {
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

static inline uint8_t rtc_bin2bcd(uint8_t bin) {
    return ((bin / 10) << 4) | (bin % 10);
}

/* Initialize RTC using LSI clock source */
static inline int rtc_hal_init(void) {
    /* 1. Enable Power Clock */
    RCC->APB1ENR1 |= RCC_APB1ENR1_PWREN;

    /* 2. Enable access to Backup Domain */
    PWR->CR1 |= PWR_CR1_DBP;

    /* 3. Enable LSI (Internal Low Speed Oscillator) */
    /* Note: In a real app, check if LSE is available first */
    RCC->CSR |= (1U << 0); /* LSION */
    while (!(RCC->CSR & (1U << 1))); /* Wait for LSIRDY */

    /* 4. Select LSI as RTC clock source and enable RTC */
    /* Note: This can only be done if RTC is not already enabled */
    if (!(RCC->BDCR & RCC_BDCR_RTCEN)) {
        RCC->BDCR |= RCC_BDCR_RTCSEL_LSI;
        RCC->BDCR |= RCC_BDCR_RTCEN;
    }

    /* 5. Disable write protection */
    RTC->WPR = RTC_WRITE_PROTECTION_KEY1;
    RTC->WPR = RTC_WRITE_PROTECTION_KEY2;

    /* 6. Enter init mode */
    RTC->ISR = 0xFFFFFFFF; /* Clear flags */
    RTC->ISR |= RTC_ISR_INIT;
    while (!(RTC->ISR & RTC_ISR_INITF));

    /* 7. Configure prescalers for 32kHz LSI */
    /* f_ck_spre = f_rtcck / ((PREDIV_S + 1) * (PREDIV_A + 1)) */
    /* 32000 / ((249+1) * (127+1)) = 1Hz */
    RTC->PRER = (127 << 16) | 249;

    /* 8. Exit init mode */
    RTC->ISR &= ~RTC_ISR_INIT;

    /* Enable write protection */
    RTC->WPR = 0xFF;
    
    return 0;
}

/* Set time registers (hours/minutes/seconds) */
static inline void rtc_hal_set_time(const rtc_time_t *time) {
    /* Unlock write protection */
    RTC->WPR = RTC_WRITE_PROTECTION_KEY1;
    RTC->WPR = RTC_WRITE_PROTECTION_KEY2;
    
    /* Enter init mode */
    RTC->ISR |= RTC_ISR_INIT;
    while (!(RTC->ISR & RTC_ISR_INITF));

    uint32_t tr = 0;
    tr |= (rtc_bin2bcd(time->hours) << 16);
    tr |= (rtc_bin2bcd(time->minutes) << 8);
    tr |= (rtc_bin2bcd(time->seconds) << 0);
    
    RTC->TR = tr;

    /* Exit init mode */
    RTC->ISR &= ~RTC_ISR_INIT;
    RTC->WPR = 0xFF;
}

/* Read time registers (hours/minutes/seconds) */
static inline void rtc_hal_get_time(rtc_time_t *time) {
    /* Wait for RSF (Registers Synchronized Flag) */
    /* Note: In high perf code, don't busy wait here indefinitely */
/* while (!(RTC->ISR & RTC_ISR_RSF)); */

    uint32_t tr = RTC->TR;
    time->hours = rtc_bcd2bin((tr >> 16) & 0x3F);
    time->minutes = rtc_bcd2bin((tr >> 8) & 0x7F);
    time->seconds = rtc_bcd2bin((tr >> 0) & 0x7F);
}

/* Set date registers (day/month/year/weekday) */
static inline void rtc_hal_set_date(const rtc_date_t *date) {
    /* Unlock write protection */
    RTC->WPR = RTC_WRITE_PROTECTION_KEY1;
    RTC->WPR = RTC_WRITE_PROTECTION_KEY2;
    
    /* Enter init mode */
    RTC->ISR |= RTC_ISR_INIT;
    while (!(RTC->ISR & RTC_ISR_INITF));

    uint32_t dr = 0;
    dr |= (rtc_bin2bcd(date->year) << 16);
    dr |= (rtc_bin2bcd(date->weekday) << 13);
    dr |= (rtc_bin2bcd(date->month) << 8);
    dr |= (rtc_bin2bcd(date->day) << 0);
    
    RTC->DR = dr;

    /* Exit init mode */
    RTC->ISR &= ~RTC_ISR_INIT;
    RTC->WPR = 0xFF;
}

/* Read date registers (day/month/year/weekday) */
static inline void rtc_hal_get_date(rtc_date_t *date) {
    uint32_t dr = RTC->DR;
    date->year = rtc_bcd2bin((dr >> 16) & 0xFF);
    date->weekday = rtc_bcd2bin((dr >> 13) & 0x07);
    date->month = rtc_bcd2bin((dr >> 8) & 0x1F);
    date->day = rtc_bcd2bin((dr >> 0) & 0x3F);
}

#endif /* RTC_HAL */
