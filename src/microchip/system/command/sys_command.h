/*
	sys_command.h is a dummy header that satisfies Harmony's calls to its own
	system command module. We have eliminated the system command module in favor
	of our own command processor.
*/


#ifndef _SYS_COMMAND_H
#define _SYS_COMMAND_H

typedef enum {
	SYS_CMD_READY,
} SYS_CMD_STATUS;
typedef struct {
	int dummy;
} SYS_CMD_INIT;

#define SYS_CMD_SINGLETON (true)
#define SYS_CMD_INIT_DEFAULTS { 0 }
#define SYS_CMD_ENABLE 0

static inline void SYS_CMD_Initialize(void) { }
static inline void SYS_CMD_Tasks(void) { }
static inline SYS_CMD_STATUS SYS_CMD_Status(void) {return SYS_CMD_READY;}

#endif
