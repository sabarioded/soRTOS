#ifndef RTC_H
#define RTC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rtc_time rtc_time_t;
typedef struct rtc_date rtc_date_t;

/**
 * @brief Initialize the RTC.
 * @return 0 on success, -1 on error.
 */
int rtc_init(void);

/**
 * @brief Read the current RTC time.
 * @param time Pointer to a rtc_time_t structure to fill.
 */
void rtc_get_time(rtc_time_t *time);

/**
 * @brief Set the RTC time.
 * @param time Pointer to a rtc_time_t structure containing the new time.
 */
void rtc_set_time(const rtc_time_t *time);

/**
 * @brief Read the current RTC date.
 * @param date Pointer to a rtc_date_t structure to fill.
 */
void rtc_get_date(rtc_date_t *date);

/**
 * @brief Set the RTC date.
 * @param date Pointer to a rtc_date_t structure containing the new date.
 */
void rtc_set_date(const rtc_date_t *date);

#ifdef __cplusplus
}
#endif

#endif /* RTC_H */
