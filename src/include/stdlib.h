#ifndef _STDLIB_H
#define _STDLIB_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

void itoa(int value, char* str, int base);
void delay(uint32_t count);

#endif