#ifndef CONSOLE_H
#define CONSOLE_H

#include "config/system/system_module.h"
#include "config/system/console/sys_console.h"

void console_putc (char c);
void console_puts (const char *s);
void console_printf(const char* fmt, ...);
void console_puthex(uint32_t value);
void console_putint(uint32_t value);
int console_getchar(void);

#endif
