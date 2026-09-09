#include "kstring.h"
int kstrcmp(const volatile char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const volatile unsigned char*)s1 - *(const unsigned char*)s2;}
