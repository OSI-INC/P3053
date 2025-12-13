/*
	cli.c -- Implementation of the Command-Line Interpreter (CLI) library.
	Provides routines that create and maintain a generic CLI that we can attach
	to any channel and expand with our own commands.

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

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>                  
#include <stdbool.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#include "configuration.h"
#include "definitions.h"
#include "utils.h"
#include "pic.h"
#include "server.h"
#include "cli.h"
#include "console.h"

/*
	cli_message writes a null-terminated string to a CLI channel. It checks
	every character in the string to make sure that all CR and LF become a
	contiguous CRLF. The routine takes as arguments a channel pointer and
	a pointer to the string. It writes the string one character at a time
	to the output, using the cli_putchar routine.
*/
void cli_message(cli_chan_type *ch, const char *s) {
    bool expect_lf = false;
    while (*s) {
        char c = *s;
        if (c == '\r') {
            ch->putchar(ch->context, '\r');
            ch->putchar(ch->context, '\n');
            expect_lf = true;
        }
        else if (c == '\n') {
            if (!expect_lf) {
        		ch->putchar(ch->context, '\r');
        		ch->putchar(ch->context, '\n');
            }
            expect_lf = false;
        }
        else {
            ch->putchar(ch->context, c);
            expect_lf = false;
        }
        s++;
    }
    ch->flush(ch->context);
}

/*
	cli_print composes a string of characters based upon a string containing
	text and formatting characters, followed by zero or more arguments, and
	prints the string it composes to the output of a CLI channel. For each
	literal argument there must be a conversion specification in the string. The
	conversion specifier begins with a percent symbol. Simple examples are
	percent symbol followed by: "d" for signed integer, "u" for unsigned
	integer, "x" for hexadecimal, "s" for a null-terminated string, "c" for a
	character, "f" for a floating point number or or a double-length floating
	point number. In the case of the "f" format specifier, we can have
	percent-symbol followed by "10.3f" for width ten characters, padded with
	spaces on the left, and three digits after the decimal point. The routine
	uses the machinery provided by stdarg.h to go through the format
	specficiations and literal arguments. This machinery is: va_start, va_list,
	vsnprintf, and va_end.
*/
void cli_print(cli_chan_type* ch, const char* fmt, ...) {
    char buff[511];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buff, sizeof(buff), fmt, args);
    va_end(args);
    cli_message(ch, buff);
}

/*
	Command line interpreter (CLI) command list. See the accompanying header file
	for the declaration of the command procedure type. We keep track of the number
	of commands registered so we will know where to register the next command in
	our list, and so we will be able to avoid overflowing the list.
*/
static cli_command_entry cli_commands[CLI_MAX_COMMANDS];
static int cli_num_commands = 0;

/*
	cli_cmd_register adds a command to our command-line interface (CLI). We pass
	name of the command and a pointer to the procedure that implements the
	command. The command must all be of type cli_cmd_proc, and it must obey the
	CLI command procedure rules. The registration routine returns 0 for success,
	-1 for a null argument, and -2 if the command list is full.
*/
int cli_cmd_register(const char *name, cli_cmd_proc proc) {
    if (name == NULL || proc == NULL) {
        return -1;
    }
    if (cli_num_commands >= CLI_MAX_COMMANDS) {
        return -2; 
    }
    cli_commands[cli_num_commands].name = name;
    cli_commands[cli_num_commands].proc = proc;
    cli_num_commands++;
    return 0;
}

/*
	cli_cmd_help with no arguments prints to the specified channel a list of all
	registered commands and their information strings. If we pass it "--info" it
	prints its info string only. If we pass it "--help" it prints its own help
	string and nothing else. We register this routine with the CLI in order to
	implement the help command within the interpreter. Look to the code below
	for an example format for the help text.
*/
void cli_cmd_help(cli_chan_type *ch, const char *args) {

	// We copy the argument list into our own local buffer. Now we can apply the
	// string-parsing function "strtok" to the buffer. This function allows us
	// to extract one word, or "token", after another from the argument list. If
	// we encounter an invalid argument, or an invalid combination of arguments,
	// we print an error to the channel and exit.
	static char args_copy[CLI_ARGS_SIZE];
	strncpy(args_copy, args, sizeof(args_copy) - 1);
	char *tok = strtok(args_copy, " \t");
	char *opt = NULL;
	
	// If tok is not null, we have found an argument. If valid, look for another
	// argument, and so on, until we either encounter an argument list error or
	// reach the end of the arguments. By the end, if we have one or more valid
	// arguments, opt will contain a string that controls the command after the
	// end of the token-sorting loop.
	while (tok != NULL) {
 		if (strcmp(tok, "--info") == 0) {
            if (opt != NULL) {
                cli_print(ch, "ERROR: Multiple options not allowed in %s.\r\n",
                	__func__);
                return;
            }
            opt = "--info";
        } else if (strcmp(tok, "--help") == 0) {
            if (opt != NULL) {
                cli_print(ch, "ERROR: Multiple options not allowed in %s.\r\n",
                	__func__);
                return;
            }
            opt = "--help";
        } else {
            cli_print(ch, "ERROR: Unrecognized option '%s' in %s.\r\n",
            	tok, __func__);
            return;
        }
        tok = strtok(NULL, " \t");
    }
    
	// The --info option must be implemented by every CLI command. In
	// response, the command must print an information string to the CLI
	// channel.
	if ((opt != NULL) && strcmp(opt, "--info") == 0) {
		cli_message(ch, "List all commands with descriptions.\r\n");
		return;
	}

	// The --help option must be supported by all CLI commands. It prints a
	// longer help message that acts as a manual page for the command, listing
	// all options and their functions.
	if ((opt != NULL) && strcmp(opt, "--help") == 0) {
		cli_message(ch,
			"Usage:\r\n"
			"  help [--info] [--help]\r\n"
			"\r\n"
			"Summary:\r\n"
			"  List all registered commands and show their '--info' descriptions.\r\n"
			"\r\n"
			"Options:\r\n"
			"  --info        Print a one-line summary of this command.\r\n"
			"  --help        Print this help text.\r\n"
		);
		return;
	}

	// If we arrive here, then w
    cli_message(ch, "Available commands:\r\n");
    for (int i = 0; i < cli_num_commands; i++) {
        const char *name = cli_commands[i].name;
        cli_cmd_proc proc = cli_commands[i].proc;
        cli_print(ch, "  %-16s ", name);
        proc(ch, "--info");
    }

    return;
}

/*
	cli_cmd_find takes a command name and returns a pointer to the procedure that 
	implements the functionality of the named command.
*/
static cli_cmd_proc cli_cmd_find(const char *name) {
    for (int i = 0; i < cli_num_commands; i++) {
        if (strcmp(cli_commands[i].name, name) == 0) {
            return cli_commands[i].proc;
        }
    }
    return NULL;
}

/*
	cli_initialize initializes the command-line interpreter. It registers a minimal
	set of commands that we believe all interpreters need.
*/
void cli_initialize(void) {
	cli_cmd_register("help",cli_cmd_help);
}

/*
	cli_start starts up a new command-line interpreter. We pass it a completed
	channel structure. It clears the receive buffer in this structure and it
	prints an introductory message. With any luck we have already called the CLI
	initialization routine by the time we start this CLI, so the help command
	will be installed in all CLI server.
*/
void cli_start(cli_chan_type* ch) {
	ch->rx_len = 0;
	ch->rx_buff[0] = '\0';
	cli_print(ch, "\r\n\r\n");
	cli_print(ch, "===========================================================\r\n");
	cli_print(ch, "=========    Embedded Ethernet Module (A3053)     =========\r\n");
	cli_print(ch, "===========================================================\r\n");
	cli_print(ch, "Command-line interpreter running, try 'help' for help.\r\n");
	cli_print(ch, "$ ");
}

/*
	cli_server maintains a command-line interpreter (CLI) on the specified
	channel. It does not block except to flush output throught the channel. It
	does not return an error code. The channel server, if one exists, must check
	the channel health before or after calling the server. To start up the
	server, we fill in a channel descriptor with the three functions that permit
	reading and writing from an active and open channel, and call cli_start.
	After that, we call cli_server in our main loop to maintain the CLI on the
	channel.
*/
void cli_server(cli_chan_type *ch) {

	// Static buffers are persistent, but we do not use them to carry
	// information from one execution of this routine to the next. We merely
	// avoiding re-allocating stack space for these buffers every time we call
	// the server routine. Note that these lengths include the null character we
	// put at the end of the command name or argument list.
    static char cmd[CLI_CMD_SIZE];
    static char args[CLI_ARGS_SIZE];
	

	// Read all available bytes into the channel's receive buffer.
    while (ch->rx_len < CLI_RX_SIZE - 1) {
    	int c = ch->getchar(ch->context);
    	if (c < 0) break;
		ch->rx_buff[ch->rx_len] = (char) c;
		ch->rx_len++;
		ch->rx_buff[ch->rx_len] = '\0';
    }

	// If we have no characters at all, return now.
	if (ch->rx_len == 0) return;

	// If the buffer has overflowed, reset it and report an error.
	if (ch->rx_len >= CLI_RX_SIZE) {
		cli_print(ch,"ERROR: Receive buffer overflow, "
			"discarding all characters in %s.\r\n", ch->name);
		ch->rx_len = 0;
		ch->rx_buff[0] = '\0';
		return;
	}
	
	// Look for a newline in the characters we have available. If we find one, mark
	// its location. If we don't find one, return.
    uint32_t i;
    for (i = 0; i < ch->rx_len; i++) {
        if (ch->rx_buff[i] == '\n' || ch->rx_buff[i] == '\r') break;
    }
    if (i == ch->rx_len) return;

	// Figure out how many newline characters there are: we might have CR, or
	// LF, or a combined CRLF. We even account for the non-standard LFCR
	// sequence.
    uint32_t eol = i;
    uint32_t consume = 1;
    if (ch->rx_buff[i] == '\r' 
    		&& i < ch->rx_len - 1 
    		&& ch->rx_buff[i+1] == '\n') {
        consume = 2;
    } else if (ch->rx_buff[i] == '\n' 
    		&& i < ch->rx_len - 1 
    		&& ch->rx_buff[i+1] == '\r') {
        consume = 2;
    }

    // Skip over any leading spaces or tabs in the input line.
	uint32_t p = 0;
	while (p < eol && (ch->rx_buff[p] == ' ' || ch->rx_buff[p] == '\t')) p++;

    // Copy the first word in the input line into our command name buffer. If we
    // overflow the name buffer, because the name is too long, discard the
    // entire receive buffer and return.
	uint32_t t = 0;
	while (p < eol && ch->rx_buff[p] != ' ' && ch->rx_buff[p] != '\t') {
		if (t >= sizeof(cmd) - 1) {
			cli_print(ch,"ERROR: Command name buffer overflow, "
				"discarding all characters in %s.\r\n", ch->name);
			ch->rx_len = 0;
			ch->rx_buff[0] = '\0';
			return;
		}
		cmd[t] = ch->rx_buff[p];
		t++;
		p++;
	}
	cmd[t] = '\0';

	// Skip over any leading spaces or tabs after the command name.
	while (p < eol && (ch->rx_buff[p] == ' ' || ch->rx_buff[p] == '\t')) p++;

	// Copy the rest of the input line into our argument list buffer. Terminate
	// with a null character. If the argument list buffer overflows, discard the
	// entire receive buffer and return an error. 
	uint32_t l = 0;
	while (p < eol) {
		if (l >= sizeof(args) - 1) {
			cli_print(ch,"ERROR: Argument list buffer overflow, "
				"discarding all characters in %s.\r\n", ch->name);
			ch->rx_len = 0;
			ch->rx_buff[0] = '\0';
			return;
		}
		args[l] = ch->rx_buff[p];
		l++;
		p++;
	}
	args[l] = '\0';
	
	// In debug mode, we report the command, arguments, and channel name.
    if (debug) console_print("[%s] %s %s\r\n", ch->name, cmd, args);

	// Shift any remaining characters all the way to the start of the receive
	// buffer. We will deal with them the next time we call this routine.
	uint32_t shift = eol + consume;
	uint32_t remain = ch->rx_len - shift;
	if (remain > 0) memmove(ch->rx_buff, &ch->rx_buff[shift], remain);
	ch->rx_len = remain;
	ch->rx_buff[ch->rx_len] = '\0';

    // See if we can find a command procedure with a name that matches our
    // command name. If not, print an error and return.
    cli_cmd_proc proc = NULL;
	proc = cli_cmd_find(cmd);
    if (proc == NULL) {
        cli_print(ch, "ERROR: Unknown command '%s' in %s.\r\n", cmd, __func__);
        return;
    }

    // Execute the command procedure, passing it the channel descriptor and the
    // argument list The command procedure reports its own errors, so does
    // not return an error or success code.
	proc(ch, args);

    // Print the prompt.
    cli_message(ch, "$ ");
}

