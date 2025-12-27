/*
	cli.h -- Interface of the Command-Line Interpreter (CLI) library. The
	passage below is an AI-generated description of the CLI purpose and
	implementation.

	Command-Line Interpreter (CLI)
	==============================
	
	This module implements a lightweight, text-based command-line interpreter
	intended for use in embedded systems. The CLI provides a uniform interface
	for interacting with the system through multiple communication channels,
	while keeping command parsing and execution independent of the underlying
	transport mechanism.
	
	Channel Abstraction
	-------------------
	
	The CLI operates on an abstract "channel" descriptor supplied by the caller.
	A channel encapsulates the primitives required for character-based I/O:
	
	- getchar   : obtain the next input character, if available
	- putchar   : transmit a single output character
	- flush     : ensure pending output is transmitted
	
	The CLI itself is unaware of how characters are transported. This allows the
	same interpreter and command set to be used over different physical or
	logical interfaces without modification. Two example channel implementations
	provided by the system are the UART2 console and Telnet connection.

	UART2 console: A character-at-a-time serial interface with local echo
	enabled. The CLI performs line editing (including backspace handling) and
	echoes input characters as they are received.

	Telnet connection: A TCP-based channel intended for remote access. Input is
	typically line-buffered by the Telnet client, and local echo is disabled on
	the server side. Telnet protocol control sequences are filtered by the
	channel's getchar implementation so that the CLI receives a clean character
	stream.
	
	Input Handling Model
	--------------------
	
	The CLI accumulates input characters into an internal receive buffer until a
	complete line is received. Lines are then tokenized and dispatched to
	registered command handlers. After processing a line, the CLI immediately
	resumes reading from the channel, allowing multiple commands to be handled
	without blocking.

	Backspace and delete characters are handled as input is received, so the CLI
	behaves correctly whether the channel delivers characters immediately or in
	newline-buffered blocks.
	
	Command History
	---------------
	
	The CLI intentionally does not implement command history or cursor
	navigation (such as up- and down-arrow recall). These features are highly
	dependent on terminal behavior and transport protocol, and are more
	appropriately handled on the client side.

	When command history or advanced line editing is desired over a Telnet or
	TCP connection, it can be provided externally using tools such as `rlwrap`,
	which add readline-style editing and history without requiring any changes
	to the embedded firmware.
	
	Overall Design Goals
	--------------------
	
	The CLI is designed to be:
	
	- Simple and predictable
	- Safe for use in embedded environments
	- Independent of transport details
	- Suitable for both interactive use and scripting
	
	This module provides the core interpreter logic. Command registration,
	channel setup, and command implementations are handled elsewhere.

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
	The Command-Line Interpreter (CLI) communication prototypes. Each CLI instance
	uses a set of routines matching these prototypes to manage its communication
	through the channel to which it is attached. It uses these routines to
	provide print procedures for the use of the commands it calls. The CLI is
	text-only. When a command wants to convey binary data, it must do so with a
	text encoding, such as using two hexadecimal digits per byte. The CLI reads
	and writes only one character at a time. It needs only three routines to
	support its use of a channel: getchar, putchar, and flush. All three return
	an integer. A positive value, including a zero, indicate no error was
	encountered. A negative value is an error code. The getchar routine must
	return a byte value 0-255 if it succeeds, -1 if there was no byte available,
	and -2 if the channel has closed. The getchar routine must not return
	characters for which cli_accept_char returns false. If getchar is reading
	from a channel that contains control characters, such as a Telnet interface,
	it must be adapted to this interface so that it removeds these control
	characters and leaves only the command-line contents that the CLI knows how
	to interpret. The putchar routine must return a 1 if it was successful, a -1
	if the output buffer was full, and a -2 if the channel has closed. The flush
	routine must return a 0 if successful, a -1 if it times out while waiting
	for the flush to complete, and a -2 if the channel is closed. The CLI does
	not need a routine to tell it how many characters are available to be read,
	because it can call getchar until the routine returns -1. Each of the
	functions takes a generic pointer that it can use to look up the context of
	the CLI channel. If the channel is a TCP socket, for example, we can pass
	the socket handle for the context pointer, and the communication function
	can apply itself to that socket. 
*/
typedef int (*cli_getchar_func)(void *context);
typedef int (*cli_putchar_func)(void *context, char c);
typedef int (*cli_flush_func)(void *context);

/*
 	The cli_chan_status_type structure defines the status codes for CLI
 	channels. When we start a CLI that respects the EEM's security level, we
 	initialize it with status "login-none" to show that the client has not yet
 	submitted a correct password to accomplish a login, but nor has the client
 	failed to do so. When we start the console CLI, we initialize to
 	"login-pass", so that the console CLI always has the ability to modify the
 	flash configuration after a reset. The CLI commands can set the status to
 	"fault" or "close". The channel management procedure will decide what to do
 	when it sees these CLI status codes. The console CLI does nothing with
 	them.
*/
typedef enum {
    CLI_LOGIN_NONE,
    CLI_LOGIN_PASS,
    CLI_LOGIN_FAIL,
    CLI_FAULT,
    CLI_CLOSE
} cli_chan_status_type;

/*
	The one hard-coded command name the CLI recognises, although it does not
	implement the command itself unless the command under that name is
	registered with the CLI. This is a login command: it must accept a password
	and compare the passowrd to the active EEM configuration password, and
	thereafter set the channel status code accordingly. By means of this command
	and the security level, the CLI can prevent unauthorized access to its
	commands, and the server managing the CLI channel can close the channel,
	which is our preferred way of dealing with unauthorized access.
*/
#define CLI_LOGIN_CMD "login"

/*
	The cli_chan_type holds all the information our comman-line interpreter
	(CLI) needs to manage comminication over a particular channel. We call it a
	"channel descriptor". Before we start a CLI, we prepare a channel descriptor
	record. We assign getchar, putchar, and flush functions for the CLI to use
	for receiving and transmitting characters. If our receive and transmit
	routines need context, such as a TCP_SOCKET handle, we put a pointer to this
	context in the channel descriptor's context field. The channel descriptor
	contains a receive buffer and a receive length that tells the CLI the
	location of the null character in the receive buffer that marks the end of
	the current accumulating command line arriving through the channel. The
	receive buffer allows the CLI to accumulate commands until they are
	complete, and to carry multiple commands through multiple calls to the CLI
	server. A transmit buffer is not strictly necessary for the operation of the
	CLI, but we include one because it acts as a shared workspace for CLI
	commands, where they can build output strings. The status field holds a status
	code, as defined with our cli_chan_status_type.
*/
#define CLI_RX_SIZE 2048
#define CLI_TX_SIZE 2048
typedef struct {
    cli_getchar_func getchar;
    cli_putchar_func putchar;
    cli_flush_func flush;
    bool echo;
    void *context;
    char rx_buff[CLI_RX_SIZE];
    uint32_t rx_len;
    char tx_buff[CLI_TX_SIZE];
    char* name;
    cli_chan_status_type status;
} cli_chan_type;

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
	cli_cmd_proc is the prototype for CLI commands. We can register any such
	procedure with the CLI and have it join the existing set of CLI commands.
	All commands we register with the CLI adhere to this same structure. They
	take as arguments a pointer to a channel descriptor and a pointer to a
	null-terminated string. The null-terminated string will contain all
	arguments passed to the CLI following the command name, stripped of any
	leading white space. Furthermore, all CLI commands are requested conform to
	the CLI policies. All commands must support the "--info" and "--help"
	options. The "--info" option prints to the console a single-line description
	of the command, no more than 63 characters long. The "--help" option prints
	a multi-line help text with a usage grammer and option list. All CLI
	commands must be non-blocking, bounded-time operations. They may trigger
	long-running background actions, but must not wait for these actions to
	complete. Commands may modify their argument string as much as they like,
	but they must not attempt to write past the end of the string.
*/
typedef void (*cli_cmd_proc) (cli_chan_type *ch, char *args);

/*
	Each command we add to the CLI's command list is a record consisting of the
	command name that should invoke the command procedure, and a pointer to the
	command procedure. Note that we can register the same procedure under
	multiple command-line names if we want to, but we should refrain from
	listing multiple procedures under the same command-line name.
*/
typedef struct {const char *name; cli_cmd_proc proc;} cli_command_entry;
#define CLI_MAX_COMMANDS 64

/*
	Command-Line Interpreter (CLI) routines. Refer to comments in their
	definitions for instructions on how to use each routine.
*/
void cli_initialize(void);
void cli_start(cli_chan_type* ch);
void cli_server(cli_chan_type* ch);
int cli_cmd_register(const char *name, cli_cmd_proc proc);
void cli_putchar(cli_chan_type *ch, char c);
void cli_message(cli_chan_type* ch, const char* buff);
void cli_print(cli_chan_type* ch, const char* fmt, ...);

/*
	Build in and example CLI command procedures.
*/
void cli_help(cli_chan_type *ch, char *args);
void cli_status(cli_chan_type *ch, char *args);
void cli_debug(cli_chan_type *ch, char *args);
void cli_login(cli_chan_type *ch, char *args);
void cli_exit(cli_chan_type *ch, char *args);

#endif
