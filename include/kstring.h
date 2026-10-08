#ifndef KSTRING_H
#define KSTRING_H

#include <stddef.h>
#include <stdint.h>

int kstrcmp(const volatile char* s1, const char* s2);
size_t kstrlen(const char* s);

size_t kstrlcpy(char* dst, const char* src, size_t size);

void* memcpy(void* dst, const void* src, size_t n);
void* memmove(void* dst, const void* src, size_t n);
void* memset(void* dst, int value, size_t n);
int memcmp(const void* a, const void* b, size_t n);

#endif
