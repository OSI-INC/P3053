/*
	cli.h -- Implementation of the Command-Line Interpreter (CLI) library.

	(C) 2025, Kevan Hashemi, Open Source Instruments Inc.

	This program is free software: you can redistribute it and/or modify it
	under the terms of the GNU General Public License as published by the Free
	Software Foundation, either version 3 of the License, or (at your option)
	any later version.

	This program is distributed in the hope that it will be useful, but WITHOUT
	ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
	FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
	more details.

	You should have received a copy of the GNU General Public License along with
	this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef CLI_H
#define CLI_H

/*
	cli_accept_char returns true iff the character is one that the CLI knows what
	to do with. If the CLI does not know what to do with a character, it will
	discard the character. This routine is for use by channel getchar routines
	to normalize the input stream for the CLI.
*/
static inline bool cli_accept_char(unsigned char c) {
    if (c >= 32 && c <= 126) return true; // Printable characters.
    if (c == '\r' || c == '\n') return true; // Carriage return and line feed.
    if (c == '\b' || c == 127) return true; // Backspace and delete.
    return false; // Discard everything else.
}

/*
	Channel communication function prototypes for the Command-Line Interpreter
	(CLI). Each CLI instance uses a set of routines matching these prototypes to
	manage its communication through the channel to which it is attached. It
	uses these routines to provide print procedures for the use of the commands
	it calls. The CLI is text-only. When a command wants to convey binary data,
	it must do so with a text encoding, such as using two hexadecimal digits per
	byte. The CLI reads and writes only one character at a time. It needs only
	three routines to support its use of a channel: getchar, putchar, and flush.
	All three return an integer. A positive value, including a zero, indicate no
	error was encountered. A negative value is an error code. The getchar
	routine must return a byte value 0-255 if it succeeds, -1 if there was no
	byte available, and -2 if the channel has closed. The getchar routine must
	not return characters for which cli_accept_char returns false. If getchar is
	reading from a channel that contains control characters, such as a Telnet
	interface, it must be adapted to this interface so that it removeds these
	control characters and leaves only the command-line contents that the CLI
	knows how to interpret. The putchar routine must return a 1 if it was
	successful, a -1 if the output buffer was full, and a -2 if the channel has
	closed. The flush routine must return a 0 if successful, a -1 if it times
	out while waiting for the flush to complete, and a -2 if the channel is
	closed. The CLI does not need a routine to tell it how many characters are
	available to be read, because it can call getchar until the routine returns
	-1. Each of the functions takes a generic pointer that it can use to look up
	the context of the CLI channel. If the channel is a TCP socket, for example,
	we can pass the socket handle for the context pointer, and the communication
	function can apply itself to that socket. 
*/
typedef int (*cli_getchar_func)(void *context);
typedef int (*cli_putchar_func)(void *context, char c);
typedef int (*cli_flush_func)(void *context);

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
#define CLI_RX_SIZE 2048
typedef struct {
    cli_getchar_func getchar;
    cli_putchar_func putchar;
    cli_flush_func flush;
    void *context;
    char rx_buff[CLI_RX_SIZE];
    uint32_t rx_len;
    char* name;
} cli_chan_type;

/*
	All commands we register with the CLI must be of the same format. All CLI
	commands must report their own errors to the CLI channel. They take as
	arguments a pointer to a channel descriptor and a pointer to a
	null-terminated string. The null-terminated string will contain all
	arguments passed to the CLI following the command name, stripped of any
	leading white space. The procedure is not allowed to modify the argument
	string. All CLI commands must implement two options. The "--info" option
	should print to the console a single-line description of the command, no
	more than 63 characters long. The "--help" option should either print the
	same description, or a multi-line help text. In their operation, CLI
	commands must be non-blocking, bounded-time operations. They may trigger
	long-running background actions, but must not wait for these actions to
	complete.
*/
typedef void (*cli_cmd_proc) (cli_chan_type *ch, const char *args);

/*
	Each command we add to the CLI's command list is a record consisting of the
	command name that should invoke the command procedure, and a pointer to the
	command procedure. Note that we can register the same procedure under
	multiple command-line names if we want to, but we should refrain from
	listing multiple procedures under the same command-line name.
*/
typedef struct {const char *name; cli_cmd_proc proc;} cli_command_entry;
#define CLI_MAX_COMMANDS 64
#define CLI_CMD_SIZE 32
#define CLI_ARGS_SIZE 256

/*
	Command-line interpreter (CLI) routines. Refer to comments in their
	definitions for instructions on how to use each routine.
*/
void cli_initialize(void);
void cli_start(cli_chan_type* ch);
void cli_server(cli_chan_type* ch);
int cli_register_command(const char *name, cli_cmd_proc proc);
void cli_putchar(cli_chan_type *ch, char c);
void cli_message(cli_chan_type* ch, const char* buff);
void cli_print(cli_chan_type* ch, const char* fmt, ...);

#endif
