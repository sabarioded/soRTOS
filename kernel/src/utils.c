#include "utils.h"


/* Poll register until bits in mask are set */
int wait_for_flag_set(volatile uint32_t *reg_addr, uint32_t bit_mask, uint32_t timeout_cycles)
{
    uint32_t i = timeout_cycles;
    while(((*reg_addr) & bit_mask) == 0U) {
        if (i-- == 0U) {
            return -1;
        }
    }
    return 0;
}


/* Poll register until bits in mask are cleared */
int wait_for_flag_clear(volatile uint32_t *reg_addr, uint32_t bit_mask, uint32_t timeout_cycles)
{
    uint32_t i = timeout_cycles;
    while(((*reg_addr) & bit_mask) != 0U) {
        if (i-- == 0U) {
            return -1;
        }
    }
    return 0;
}


/* Poll register until masked value equals expected value */
int wait_for_reg_mask_eq(volatile uint32_t *reg_addr, 
                        uint32_t bit_mask, 
                        uint32_t expected_value, 
                        uint32_t timeout_cycles)
{
    uint32_t i = timeout_cycles;
    while(((*reg_addr) & bit_mask) != expected_value) {
        if (i-- == 0U) {
            return -1;
        }
    }
    return 0;
}


/* Simple string to integer conversion */
int utils_atoi(const char *str) {
    int res = 0;
    int digit = 0;
    while(*str && *str >= '0' && *str <= '9') {
        digit = *str - '0';
        res = res * 10 + digit;
        str++;
    }
    return res;
}


/* Fill memory with constant byte */
void *utils_memset(void *dest, int value, size_t count) {
    unsigned char *p = dest;
    while (count--) {
        *p++ = (unsigned char)value;
    }
    return dest;
}


/* Copy memory block */
void *utils_memcpy(void *dest, const void *src, size_t count) {
    unsigned char *d = dest;
    const unsigned char *s = src;
    while (count--) {
        *d++ = *s++;
    }
    return dest;
}


/* Compare two strings */
int utils_strcmp(const char *str1, const char *str2) {
    while (*str1 && (*str1 == *str2)) {
        str1++;
        str2++;
    }
    return *(const unsigned char *)str1 - *(const unsigned char *)str2;
}
