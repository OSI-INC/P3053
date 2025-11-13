#ifndef SYS_CONSOLE_H
#define SYS_CONSOLE_H

#include "system/system_module.h"

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
