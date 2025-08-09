#ifndef _STRING_H
#define _STRING_H

#include <stddef.h>

/* Standard string functions */
size_t strlen(const char* str);
int strcmp(const char* s1, const char* s2);
int strncmp(const char* s1, const char* s2, size_t n);
char* strcpy(char* dest, const char* src);
char* strncpy(char* dest, const char* src, size_t n);
void* memset(void* ptr, int value, size_t num);
void* memcpy(void* dest, const void* src, size_t n);
void* memmove(void* dest, const void* src, size_t n);

/* Additional string functions */
void strlower(char* str);
char* strtok(char* str, const char* delim);
size_t strspn(const char* str, const char* accept);
char* strpbrk(const char* str, const char* accept);

#endif