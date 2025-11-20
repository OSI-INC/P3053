/*
	console.h declares the console read and write routines, as well as the
	console-based command interface. It declares the SYS_CONSOLE family of
	read and write routines as well, bu calling sys_console.h. In console.c,
	we provide bodies for these SYS_CONSOLE routines that are used by all
	Harmony system modules.
*/

#ifndef CONSOLE_H
#define CONSOLE_H

#include "config/system/console/sys_console.h"

// Procedures that read from and write to the console.
void console_putchar(char c);
void console_flush(void);
void console_puthex(uint32_t value);
void console_putint(uint32_t value);
void console_write(const void* buff, size_t size);
void console_message(const char *s);
void console_print(const char* fmt, ...);
int console_readcount(void);
int console_getchar(void);

// Procedures that initialize and maintain the console.
void console_initialize(void);
void console_server(void);

#endif
