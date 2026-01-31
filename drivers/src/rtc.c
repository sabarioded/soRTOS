#include "rtc.h"
#include "rtc_hal.h"

int rtc_init(void) {
    return rtc_hal_init();
}

void rtc_get_time(rtc_time_t *time) {
    rtc_hal_get_time(time);
}

void rtc_set_time(const rtc_time_t *time) {
    rtc_hal_set_time(time);
}

void rtc_get_date(rtc_date_t *date) {
    rtc_hal_get_date(date);
}

void rtc_set_date(const rtc_date_t *date) {
    rtc_hal_set_date(date);
}