#ifndef RTC_HAL_NATIVE_H
#define RTC_HAL_NATIVE_H

#include "rtc.h"

int rtc_hal_init(void);
void rtc_hal_get_time(rtc_time_t *time);
void rtc_hal_set_time(const rtc_time_t *time);
void rtc_hal_get_date(rtc_date_t *date);
void rtc_hal_set_date(const rtc_date_t *date);

#endif /* RTC_HAL_NATIVE_H */
