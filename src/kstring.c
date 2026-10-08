#include "kstring.h"

int kstrcmp(const volatile char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const volatile unsigned char*)s1 - *(const unsigned char*)s2;
}

size_t kstrlen(const char* s)
{
    size_t length = 0;

    while (s[length] != '\0') {
        length++;
    }

    return length;
}

size_t kstrlcpy(char* dst, const char* src, size_t size)
{
    size_t length = kstrlen(src);

    if (size != 0) {
        size_t n = (length >= size) ? size - 1 : length;

        memcpy(dst, src, n);
        dst[n] = '\0';
    }

    return length;
}

void* memcpy(void* dst, const void* src, size_t n)
{
    void* result = dst;

    __asm__ __volatile__("cld\n\trep movsb" : "+D"(dst), "+S"(src), "+c"(n) : : "memory");

    return result;
}

void* memmove(void* dst, const void* src, size_t n)
{
    uint8_t* d = (uint8_t*)dst;
    const uint8_t* s = (const uint8_t*)src;

    if (n == 0 || d == s) {
        return dst;
    }

    if (d < s || d >= s + n) {
        return memcpy(dst, src, n);
    }

    d += n - 1;
    s += n - 1;

    __asm__ __volatile__("std\n\trep movsb\n\tcld" : "+D"(d), "+S"(s), "+c"(n) : : "memory");

    return dst;
}

void* memset(void* dst, int value, size_t n)
{
    void* result = dst;

    __asm__ __volatile__("cld\n\trep stosb" : "+D"(dst), "+c"(n) : "a"(value) : "memory");

    return result;
}

int memcmp(const void* a, const void* b, size_t n)
{
    const uint8_t* pa = (const uint8_t*)a;
    const uint8_t* pb = (const uint8_t*)b;

    while (n > 0) {
        if (*pa != *pb) {
            return (int)*pa - (int)*pb;
        }

        pa++;
        pb++;
        n--;
    }

    return 0;
}
