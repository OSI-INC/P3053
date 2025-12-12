/*
	console.h declares the console read and write routines, as well as the
	console-based command interface. It declares the SYS_CONSOLE family of read
	and write routines as well, by calling sys_console.h. In console.c, we
	provide bodies for these SYS_CONSOLE routines that are used by all Harmony
	system modules. If the VERBOSE_CONSOLE macro is defined, we set a local
	"debug" flag, which routines can use to turn on and off their reporting to
	the console.
*/

#ifndef CONSOLE_H
#define CONSOLE_H

// Include the Harmony SYS_CONSOLE routine declarations. We will be supplying 
#include "config/system/console/sys_console.h"

#ifdef VERBOSE_CONSOLE
static const bool debug = true;
#else
static const bool debug = false;
#endif

/*
	Procedures that read and write from the UART console. For now, we preserve
	our original console routines, which go straight to UART2. Eventually, we
	will replace the console with the generic and multi-channelo command line
	interpreter routines.
*/
void console_putchar(char c);
void console_flush(void);
int console_readcount(void);
int console_getchar(void);
void console_puthex(uint32_t value);
void console_putint(uint32_t value);
void console_write(const void* buff, size_t size);
void console_message(const char *s);
void console_print(const char* fmt, ...);

/*
	The console initialization and service routines.
*/
void console_initialize(void);
void console_server(void);

#endif
