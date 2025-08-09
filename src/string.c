#include "string.h"

/* Standard string functions */
size_t strlen(const char* str) {
    size_t len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}

int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return (int)((unsigned char)*s1) - (int)((unsigned char)*s2);
}

int strncmp(const char* s1, const char* s2, size_t n) {
    for (size_t i = 0; i < n; i++) {
        unsigned char c1 = (unsigned char)s1[i];
        unsigned char c2 = (unsigned char)s2[i];
        if (c1 != c2) return (int)c1 - (int)c2;
        if (c1 == '\0') return 0;
    }
    return 0;
}

char* strcpy(char* dest, const char* src) {
    char* original_dest = dest;
    while ((*dest++ = *src++));
    return original_dest;
}

char* strncpy(char* dest, const char* src, size_t n) {
    char* original_dest = dest;
    size_t i = 0;
    for (; i < n && src[i] != '\0'; i++) dest[i] = src[i];
    for (; i < n; i++) dest[i] = '\0';
    return original_dest;
}

void* memset(void* ptr, int value, size_t num) {
    unsigned char* p = ptr;
    while (num--) {
        *p++ = (unsigned char)value;
    }
    return ptr;
}

void* memcpy(void* dest, const void* src, size_t n) {
    char* d = dest;
    const char* s = src;
    while (n--) {
        *d++ = *s++;
    }
    return dest;
}

void* memmove(void* dest, const void* src, size_t n) {
    char* d = dest;
    const char* s = src;
    
    if (d < s) {
        while (n--) *d++ = *s++;
    } else {
        d += n;
        s += n;
        while (n--) *--d = *--s;
    }
    return dest;
}

/* Additional string functions */
void strlower(char* str) {
    for (; *str; str++) {
        if (*str >= 'A' && *str <= 'Z') {
            *str = *str - 'A' + 'a';
        }
    }
}

size_t strspn(const char* str, const char* accept) {
    const char* p;
    const char* a;
    size_t count = 0;
    
    for (p = str; *p != '\0'; p++) {
        for (a = accept; *a != '\0'; a++) {
            if (*p == *a) break;
        }
        if (*a == '\0') return count;
        count++;
    }
    return count;
}

char* strpbrk(const char* str, const char* accept) {
    while (*str != '\0') {
        const char* a = accept;
        while (*a != '\0') {
            if (*str == *a) return (char*)str;
            a++;
        }
        str++;
    }
    return NULL;
}

/* Tokenization functions */
static char* strtok_next = NULL;

char* strtok(char* str, const char* delim) {
    if (str) {
        strtok_next = str;
    }
    
    if (!strtok_next || !*strtok_next) {
        return NULL;
    }
    
    // Skip leading delimiters
    strtok_next += strspn(strtok_next, delim);
    if (!*strtok_next) {
        return NULL;
    }
    
    char* token = strtok_next;
    strtok_next = strpbrk(token, delim);
    
    if (strtok_next) {
        *strtok_next = '\0';
        strtok_next++;
    }
    
    return token;
}