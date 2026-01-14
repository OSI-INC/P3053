/*
	sys_console.h declares the SYS_CONSOLE_* routines that are called by the
	Microchip Library's debug reporting system. We have deleted the
	sys_console.c file and all the Microchip sys_console source files. We
	provide the SYS_CONSOLE_* routines in our own higher-level source file, so
	the routines listed below will be resolved at link time.
	
	[22-DEC-25] Kevan Hashemi, Open Source Instruments Inc.
*/

#ifndef SYS_CONSOLE_H
#define SYS_CONSOLE_H

#include "microchip/system/system_module.h"

/*
	The initialization structure for Microchip's SYS_CONSOLE system module. We
	have eliminated this module, but the other modules still need a structure to
	refer too, so we we create a dummy structure.
*/
typedef struct { } SYS_CONSOLE_INIT;

/*
	Declarations of the SYS_CONSOLE family of printing and reading procedures.
	These are routines called by the Microchip processes to print debug messages
	and to manage the command interface. We provide the implementationi of these
	procedures in our higher-level consol.c library. All these procedures pass
	as their first argument the index of the console to be written to or read
	from. We will ignore that index in our implementation of the procedures,
	because we have only one console: UART2.
*/
void SYS_CONSOLE_Message(int index, const char* msg);
void SYS_CONSOLE_Print(int index, const char* fmt, ...);

/*
	When the Microchip libraries wants to know the state us of the console, we
	will simply say that it is ready, regardless of the index that the Microchip
	routine passes. We don't want any trouble.
*/
static inline SYS_STATUS SYS_CONSOLE_Status(int index) {
    return SYS_STATUS_READY;
}

/*
	The default index for the Microchip routines to pass in its SYS_CONSOLE
	print and read procedures. We ignore this index, so it does not, in theory,
	matter what value we assign to it.
*/
#define SYS_CONSOLE_DEFAULT_INSTANCE 0

/*
	Most Microchip library routines use an all-caps version of the above
	routines. The all-caps version does not specify the console index. Here we
	define all-caps macros that transform into the above routines with index
	zero specified. The double-hash symbol before __VA_ARGS__ tells the compiler
	to tolerate and manage calls of the macro in which the code provides no
	conversion specifiers.
*/
#define SYS_CONSOLE_MESSAGE(msg) SYS_CONSOLE_Message(0, msg)
#define SYS_CONSOLE_PRINT(fmt, ...) SYS_CONSOLE_Print(0, fmt, ##__VA_ARGS__)
#define SYS_DEBUG_MESSAGE(level, msg) SYS_CONSOLE_Message(0, (msg))
#define SYS_DEBUG_PRINT(level, fmt, ...) SYS_CONSOLE_Print(0, fmt, ##__VA_ARGS__)

#endif
