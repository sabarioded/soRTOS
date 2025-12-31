#include "utils.h"


int wait_for_flag_set(volatile uint32_t *reg, uint32_t mask, uint32_t max_iter)
{
    uint32_t i = max_iter;
    while(((*reg) & mask) == 0U) {
        if (i-- == 0U) return -1;
    }
    return 0;
}


int wait_for_flag_clear(volatile uint32_t *reg, uint32_t mask, uint32_t max_iter)
{
    uint32_t i = max_iter;
    while(((*reg) & mask) != 0U) {
        if (i-- == 0U) return -1;
    }
    return 0;
}


int wait_for_reg_mask_eq(volatile uint32_t *reg, uint32_t mask, uint32_t expected, uint32_t max_iter)
{
    uint32_t i = max_iter;
    while(((*reg) & mask) != expected) {
        if (i-- == 0U) return -1;
    }
    return 0;
}


int atoi(const char *string) {
    int res = 0;
    int interim = 0;
    while(*string && *string >= '0' && *string <= '9') {
        interim = *string - '0';
        res = res * 10 + interim;
        string++;
    }
    return res;
}


void *memset(void *s, int c, size_t n) {
    unsigned char *p = s;
    while (n--) {
        *p++ = (unsigned char)c;
    }
    return s;
}


void *memcpy(void *dest, const void *src, size_t n) {
    unsigned char *d = dest;
    const unsigned char *s = src;
    while (n--) {
        *d++ = *s++;
    }
    return dest;
}


int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}
