#ifndef RTC_H
#define RTC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;
} rtc_time_t;

typedef struct {
    uint8_t day;
    uint8_t month;
    uint8_t year; /* 0-99 */
    uint8_t weekday; /* 1=Mon, 7=Sun */
} rtc_date_t;

/**
 * @brief Initialize the RTC.
 * Uses LSI clock source.
 * @return 0 on success, -1 on error.
 */
int rtc_init(void);

void rtc_get_time(rtc_time_t *time);
void rtc_set_time(const rtc_time_t *time);

void rtc_get_date(rtc_date_t *date);
void rtc_set_date(const rtc_date_t *date);

#ifdef __cplusplus
}
#endif

#endif /* RTC_H */