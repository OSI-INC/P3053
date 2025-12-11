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

/*
	Command line interface (CLI) low-level communication prototypes. When we
	create a new interpreter, we pass a channel record into it. This record
	defines how the interpreter should interact with the communication channel.
	This definition consists of four routines: readount, getbytes, putbytes, and
	flush. We declare the format of these routines below. The context pointer is
	a generic pointer to a data structure we might need to refer to when writing
	to the channel. In the case of a TCP socket, the context would be a TCP
	socket handle. The CLI commands themselves are not permitted to call any of
	these four routines. They never read from a CLI channel. They receive their
	input from the CLI directly, not by reading from the channel. The CLI
	commands can and should write to the CLI channel, but they must do so with
	the cli_putchar, cli_message and cli_print routines, declared farther down.
	These four routines all return integers. In the case of readcount, the
	return is the number of bytes available, or a negative value for error. For
	getbytes the return is the number of bytes read, or negative for an error.
	For butbytes it is the number of bytes written, or negative for error. For
	flush it is a zero for success and negative for error.
*/
typedef int (*cli_readcount_func)(void *context);
typedef int (*cli_getbytes_func)(void *context, uint8_t* buff, uint32_t len);
typedef int (*cli_putbytes_proc)(void *context, const uint8_t* buff, uint32_t len);
typedef int (*cli_flush_proc)(void *context);

/*
	The cli_chan_type holds all the information one of our comman-line
	interpreter (CLI) needs to manage comminication over a particular channel.
	When we set up a CLI, we compose the channel structure and pass a pointer to
	the record into the CLI initialization routine. When we call the CLI to
	service the channel, we pass the same the same pointer into the CLI server
	routine. In addition to four channel-specific read and write routines, we
	have a receive buffer and receive length counter. The receive buffer stores
	bytes as they arrive. It allows the CLI to accumulate commands until they
	are complete, and to carry multiple commands through multiple calls to the
	CLI server. We do not include a transmit buffer because this can be created
	by the CLI for each command response. 
*/
#define CLI_RX_SIZE 2047
typedef struct {
    cli_readcount_func readcount;
    cli_getbytes_func getbytes;
    cli_putbytes_proc putbytes;
    cli_flush_proc flush;
    void *context;
    uint8_t rx_buff[CLI_RX_SIZE];
    uint32_t rx_len;   
} cli_chan_type;

/*
	All commands we register with the CLI must take be of the same format. The
	only command reserved by the CLI is "help". All CLI commands must return an
	integer status code and take as arguments a pointer to a channel descriptor,
	a pointer to a null-terminated string, and no other arguments. The error
	codes must be: 0 for success, -1 for a syntax error or invalid arguments, -2
	for an error encountered during execution, and -3 for a request that cannot
	be granted because of the state of the system. The null-terminated string
	will include the command line received by the CLI, but stripped of the name
	of the command and any whitespace after the name, so that the first token in
	the string is the first command-line argument. The procedure is not allowed
	to modify the argument string. All CLI commands must implement two options.
	The "--info" option should print to the console a single-line description of
	the command, no more than 63 characters long. The "--help" option should
	either print the same description, or a multi-line help text. In their
	operation, CLI commands must be non-blocking, bounded-time operations. They
	may trigger long-running background actions, but must not wait for these
	actions to complete.
*/
typedef int (*cli_cmd_proc) (
	cli_chan_type *ch,
	const char *args);

/*
	Each command we add to the CLI's command list is a record consisting of the
	command name that should invoke the command procedure, and a pointer to the
	command procedure. Note that we can register the same procedure under
	multiple command-line names if we want to, but we should refrain from
	listing multiple procedures under the same command-line name.
*/
typedef struct {
    const char *name; 
    cli_cmd_proc proc;
} cli_command_entry;
#define CLI_MAX_COMMANDS 64

/*
	The command-line interpreter (CLI) routines.
*/
int cli_register_command(const char *name, cli_cmd_proc proc);
void cli_putchar(cli_chan_type *ch, char c);
void cli_message(cli_chan_type* ch, const char* buff);
void cli_print(cli_chan_type* ch, const char* fmt, ...);
void cli_initialize(cli_chan_type* ch);
void cli_server(cli_chan_type* ch);

#endif
