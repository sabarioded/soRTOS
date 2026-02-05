#ifndef RTC_HAL_NATIVE_H
#define RTC_HAL_NATIVE_H

#include "rtc.h"

struct rtc_time {
    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;
};

struct rtc_date {
    uint8_t day;
    uint8_t month;
    uint8_t year; /* 0-99 */
    uint8_t weekday; /* 1=Mon, 7=Sun */
};

int rtc_hal_init(void);
void rtc_hal_get_time(rtc_time_t *time);
void rtc_hal_set_time(const rtc_time_t *time);
void rtc_hal_get_date(rtc_date_t *date);
void rtc_hal_set_date(const rtc_date_t *date);

#endif /* RTC_HAL_NATIVE_H */
