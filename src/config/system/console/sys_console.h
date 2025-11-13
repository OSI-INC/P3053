/*
	sys_console.h declares the SYS_CONSOLE_* routines that are called by
	Harmony's debug reporting system. We have deleted the sys_console.c file and
	all the Harmony sys_console source files. We provide the SYS_CONSOLE_*
	routines in our own higher-level source file, so the routines listed below
	will be resolved at link-time.
	
	[13-NOV-25] Kevan Hashemi, Open Source Instruments Inc.
*/

#ifndef SYS_CONSOLE_H
#define SYS_CONSOLE_H

#include "config/system/system_module.h"

typedef struct
{
} SYS_CONSOLE_INIT;

void SYS_CONSOLE_Print(int index, const char* fmt, ...);
void SYS_CONSOLE_Message(int index, const char* msg);
void SYS_CONSOLE_Write(int index, const void* buff, size_t size);
void SYS_CONSOLE_Read(int index, void* buff, size_t size);
void SYS_CONSOLE_Tasks(int index);
void SYS_CONSOLE_Task(int index);
int SYS_CONSOLE_ReadCountGet(int index);

static inline SYS_STATUS SYS_CONSOLE_Status(int index)
{
    return SYS_STATUS_READY;
}

#define SYS_CONSOLE_DEFAULT_INSTANCE 0

#endif
