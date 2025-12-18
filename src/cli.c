/*
	cli.c -- Implementation of the Command-Line Interpreter (CLI) library. 

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
	cli_start starts up a new command-line interpreter. We pass it a completed
	channel structure. It clears the receive buffer in this structure and it
	prints an introductory message. With any luck we have already called the CLI
	initialization routine by the time we start this CLI, so the help command
	will be installed in all CLI server.
*/
void cli_start(cli_chan_type* ch) {
	ch->rx_len = 0;
	ch->rx_buff[0] = '\0';
	cli_print(ch, "===========================================================\n");
	cli_print(ch, "====  Embedded Ethernet Module Command-Line Interface =====\n");
	cli_print(ch, "===========================================================\n");
	cli_print(ch, "Command-line interpreter running, try 'help' for help.\n");
}

/*
	Command-Line Interpreter (CLI) command list. See the accompanying header file
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
	cli_server maintains a Command-Line Interpreter (CLI) on the specified
	channel. It does not block except to flush output throught the channel. It
	does not return an error code. The channel server, if one exists, must check
	the channel health before or after calling the server. To start up the
	server, we fill in a channel descriptor with the three functions that permit
	reading and writing from an active and open channel, and call cli_start.
	After that, we call cli_server in our main loop to maintain the CLI on the
	channel.
*/
void cli_server(cli_chan_type *ch) {
	const char* prompt = "EEM$ ";

	// Static buffers are persistent, but we do not use them to carry
	// information from one execution of this routine to the next. We merely
	// avoiding re-allocating stack space for these buffers every time we call
	// the server routine. Note that these lengths include the null character we
	// put at the end of the command name or argument list.
    static char cmd[CLI_CMD_SIZE];
    static char args[CLI_ARGS_SIZE];

	// Read bytes into the channel's receive buffer until we see an end of line.
	// We execute backspace and delete immediately we see them. If echo is
	// enabled, we echo all characters, with backspace and delete being echoed
	// as "backspace space backspace" in order to remove the most-recently
	// echoed character from view. We handle CR, LF, and CRLF with the
	// assumption that we will never get LFCR, or if we do: we will handle it as
	// two line breaks. When we see CR, we set our ignore_lf flag. We will now
	// try to read the next character from the channel. If no character is
	// waiting, we assume no LF is coming, and we proceed to process the line we
	// have. If a LF is waiting, we discard it and process our line.
	bool ignore_lf = false;
	bool process_command = false;    
    while (!process_command || ignore_lf) {
    	int c = ch->getchar(ch->context);
    	if (c < 0) {
    		ignore_lf = false;
    		break;
    	} else if (c == '\r') {
			process_command = true;
			ignore_lf = true;
		} else if (c == '\n') {
			if (!ignore_lf) {
				process_command = true;
			} else {
				ignore_lf = false;
			}
		} else if (c == '\b' || c == 0x7F) {
			if (ch->rx_len > 0) {
				ch->rx_len--;
				if (ch->echo) cli_message(ch, "\b \b");
			}
		} else if (c >= 32 && c <= 126) {
			ch->rx_buff[ch->rx_len] = c;
			ch->rx_len++;
			ch->rx_buff[ch->rx_len] = '\0';
			if (ch->echo) cli_print(ch, "%c", c);
		}
		if (ch->rx_len >= CLI_RX_SIZE - 1) {
			cli_print(ch,"ERROR: Receive buffer overflow in %s.\n", ch->name);
			ch->rx_len = 0;
			ch->rx_buff[0] = '\0';
			return;			
		}
	}
 
 	// If we are not going to process a command, return
	if (!process_command) return;
	
	// If we are echoing characters, print a newline to terminate the command line.
	if (ch->echo) cli_message(ch, "\n");

	// If we have no characters at all, print the prompt and return.
	if (ch->rx_len == 0) {
		cli_message(ch, prompt);
		return;
	}
	
    // Skip over any leading spaces or tabs in the input line.
	uint32_t p = 0;
	while (ch->rx_buff[p] == ' ' || ch->rx_buff[p] == '\t') p++;

    // Copy the first word in the input line into our command name buffer. If we
    // overflow the name buffer, because the name is too long, discard the
    // entire receive buffer and return.
	uint32_t t = 0;
	while (ch->rx_buff[p] != '\0' 
			&& ch->rx_buff[p] != ' ' 
			&& ch->rx_buff[p] != '\t') {
		if (t >= sizeof(cmd) - 1) {
			cli_print(ch,"ERROR: Command name buffer overflow, "
				"discarding all characters in %s.\n", ch->name);
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
	while (ch->rx_buff[p] == ' ' || ch->rx_buff[p] == '\t') p++;

	// Copy the rest of the input line into our argument list buffer. Terminate
	// with a null character. If the argument list buffer overflows, discard the
	// entire receive buffer and return an error. 
	uint32_t l = 0;
	while (ch->rx_buff[p] != '\0') {
		if (l >= sizeof(args) - 1) {
			cli_print(ch,"ERROR: Argument list buffer overflow, "
				"discarding all characters in %s.\n", ch->name);
			ch->rx_len = 0;
			ch->rx_buff[0] = '\0';
			return;
		}
		args[l] = ch->rx_buff[p];
		l++;
		p++;
	}
	args[l] = '\0';

	// Move the receive buffer pointer back to the beginning of the buffer and
	// load a null there as well.
	ch->rx_len = 0;
	ch->rx_buff[0] = '\0';
	
	// If the command name is an empty string, print the prompt and return.
	if (strlen(cmd) == 0) {
	    cli_message(ch, prompt);
		return;
	}

    // See if we can find a command procedure with a name that matches our
    // command name. If not, print an error and return.
    cli_cmd_proc proc = NULL;
	proc = cli_cmd_find(cmd);
    if (proc == NULL) {
        cli_print(ch, "ERROR: Unknown command '%s' in %s.\n", cmd, __func__);
	    cli_message(ch, prompt);
        return;
    }

    // Execute the command procedure, passing it the channel descriptor and the
    // argument list The command procedure reports its own errors, so does
    // not return an error or success code.
	proc(ch, args);

    // Print the prompt and we are done.
    cli_message(ch, prompt);
    return;
}

/*
	cli_cmd_template is a template for us to copy when making new commands. It 
	contains the essential logic and arguments, as well as comments.
*/
void cli_cmd_template(cli_chan_type *ch, char *args) {
	bool print_info = false;
	bool print_help = false;

	// We apply the string-parsing function "strtok" to the argument string.
	// This function allows us to extract one word, or "token", after another
	// from the argument list. If we encounter an invalid argument, or an
	// invalid combination of arguments, we print an error to the channel and
	// exit.
	char* tok = strtok(args, " \t");

	// If tok is not null, we have found an argument. If the argument is valid,
	// set a flag. If invalid, print an error message and return.
	while (tok != NULL) {
 		if (strcmp(tok, "--info") == 0) {print_info = true;} 
 		else if (strcmp(tok, "--help") == 0) {print_help = true;} else {
            cli_print(ch, "ERROR: Unrecognized option '%s' in %s.\n", tok, __func__);
            return;
        }
        tok = strtok(NULL, " \t");
    }
    
	// The --info option must be implemented by every CLI command. In
	// response, the command must print an information string to the CLI
	// channel. It short-circuits the command: any time we see this option,
	// we print description and return.
	if (print_info) {
		cli_message(ch, "List all commands with descriptions.\n");
		return;
	}

	// The --help option must be supported by all CLI commands. It prints a
	// longer help message that acts as a manual page for the command, listing
	// all options and their functions. This option short-circuits the command
	// just like the --info option: if present, no other action will be taken,
	// other than to print this message. We move the message text all the way
	// to the left edge of the screen so it is easier to fill out and we can
	// use the full eighty-character width.
	if (print_help) {
		cli_message(ch,
"Usage:\n"
"  template [--info] [--help]\n"
"\n"
"Summary:\n"
"  Template desription.\n"
"\n"
"Options:\n"
"  --info        Print a one-line summary of this command.\n"
"  --help        Print this help text.\n"
		);
		return;
	}

	// This is where we put the dispatch commands.
	cli_print(ch, "Template command executed.");

    return;
}

/*
	cli_help with no arguments prints to the specified channel a list of all
	registered commands and their information strings. If we pass it "--info" it
	prints its info string only. Otherwise, if we pass it "--help", it prints
	its own help string.
*/
void cli_help(cli_chan_type *ch, char *args) {
	bool print_info = false;
	bool print_help = false;

	char* tok = strtok(args, " \t");
	
	while (tok != NULL) {
 		if (strcmp(tok, "--info") == 0) {print_info = true;} 
 		else if (strcmp(tok, "--help") == 0) {print_help = true;} else {
            cli_print(ch, "ERROR: Unrecognized option '%s' in %s.\n", tok, __func__);
            return;
        }
        tok = strtok(NULL, " \t");
    }
    
	if (print_info) {
		cli_message(ch, "List all commands with descriptions.\n");
		return;
	}

	if (print_help) {
		cli_message(ch,
"Usage:\n"
"  help [--info] [--help]\n"
"\n"
"Summary:\n"
"  List all registered commands and show their '--info' descriptions.\n"
"\n"
"Options:\n"
"  --info        Print a one-line summary of this command.\n"
"  --help        Print this help text.\n"
		);
		return;
	}

    cli_message(ch, "Available commands:\n");
    for (int i = 0; i < cli_num_commands; i++) {
        const char *name = cli_commands[i].name;
        cli_cmd_proc proc = cli_commands[i].proc;
        cli_print(ch, "  %-16s ", name);
        proc(ch, "--info");
    }

    return;
}



