#ifndef CONSOLE_H
#define CONSOLE_H

#include "config/system/system_module.h"
#include "config/system/console/sys_console.h"

void console_putchar(char c);
void console_puthex(uint32_t value);
void console_putint(uint32_t value);
void console_write(const void* buff, size_t size);
void console_message(const char *s);
void console_print(const char* fmt, ...);
int console_readcount(void);
int console_getchar(void);

#endif
