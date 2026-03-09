/*
	cli.c -- Implementation of the Command-Line Interpreter (CLI) library. 

	Copyright (C) 2025-2026, Kevan Hashemi, Open Source Instruments Inc.

	This program is free software: you can redistribute it and/or modify it
	under the terms of the GNU General Public License as published by the Free
	Software Foundation, either version 3 of the License, or (at your option)
	any later version.

	This program is distributed in the hope that it will be useful, but WITHOUT
	ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
	FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
	more details.

	You should have received a copy of the GNU General Public License along with
	this program. If not, see <https://www.gnu.org/licenses/>.
*/

/*
	The standard C headers we trust the compiler will know how to find. The
	Microchip library headers are configuration.h and definitions.h. The
	compiler must be told where to look for these two files. The remaining
	interfaces are those that go with our EEM implementation files.
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
#include "config.h"
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
void cli_print(cli_chan_type *ch, const char *fmt, ...) {
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
void cli_start(cli_chan_type *ch) {
	ch->rx_len = 0;
	ch->rx_buff[0] = '\0';
	cli_print(ch, "===========================================================\n");
	cli_print(ch, "===  Embedded Ethernet Module Command-Line Interpreter ====\n");
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
	cli_cmd_register adds a command to our command-line interpreter (CLI). We pass
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
	reading and writing from an active and open channel. We set the channel
	status field to either LOGIN_NONE or LOGIN_PASS. We enable or disable
	character echo with the echo field. After that, we call cli_server in our
	main loop to maintain the CLI on the channel. The server has one hard-coded
	command it recognises: the login command, named CLI_LOGIN_CMD. If the active
	EEM configuration's security level field is 2 or greater, and the channel
	status is not equal to LOGIN_PASS, the server will discard the command and
	return with status CLI_FAULT.
*/
void cli_server(cli_chan_type *ch) {
	// We are going to use the existing channel receive buffer as our work space
	// for commands operating upon incoming lines. The cmd pointer will point to
	// the first character of the command name, and we will make sure a NULL
	// sits just after the command name. We will use the args pointer in the
	// same way for the argument list.
	char *cmd;
	char *args;
	
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
	    cli_print(ch, "%s", ch->prompt);	
		return;
	}
	
    // Skip over any leading spaces or tabs in the input line to find the first
    // character of the command.
	uint32_t p = 0;
	while (ch->rx_buff[p] == ' ' || ch->rx_buff[p] == '\t') p++;
	cmd = &ch->rx_buff[p];

    // Find the space, tab, or null at the end of the command name. If it is a
    // null, then this is the end of the command and there are no arguments. If
    // it is a space or tab, we might have arguments. Replace the space or tab
    // with a null and move to the next character.
	while (ch->rx_buff[p] != '\0' 
			&& ch->rx_buff[p] != ' ' 
			&& ch->rx_buff[p] != '\t') {
		p++;
	}
	if (ch->rx_buff[p] != '\0') {
		ch->rx_buff[p] = '\0';
		p++;
	}

	// Skip over any leading spaces or tabs after the command name to find the first
	// character of the arguments. We already have a null at the end of the argument
	// list, so we have now isolated the arguments.
	while (ch->rx_buff[p] == ' ' || ch->rx_buff[p] == '\t') p++;
	args = &ch->rx_buff[p];

	// If the active EEM configuration security level is greater than one, and
	// the name of the command we are about to execute is not CLI_LOGIN_CMD, we
	// print an error message and set the channel status to CLI_FAULT.
	if (eem_config_active.seclevel > 1 
			&& ch->status != CLI_PASS
			&& strcmp(cmd, CLI_LOGIN_CMD) != 0) {
		cli_print(ch,"ERROR: Access denied, login required in %s.\n", ch->name);
		ch->status = CLI_FAULT;
	} else {

    // See if we can find a command procedure with a name that matches our
    // command name. If we find the procedure, execute it, passing it the
    // channel descriptor and the argument list pointer. Command procedures
    // report their own errors to the client directly. If we don't find the
    // procedure, print an error message.
		cli_cmd_proc proc = NULL;
		proc = cli_cmd_find(cmd);
		if (proc != NULL) {
			proc(ch, args);
		} else {
			cli_print(ch, "ERROR: Unknown command '%s' in %s.\n", cmd, __func__);
		}
	}	

	// Move the receive buffer pointer to the beginning of the buffer and load a
	// null there as well.
	ch->rx_len = 0;
	ch->rx_buff[0] = '\0';
		
    // We are done. The cursor should be back at the beginning of a new line.
    // We print the prompt and return.
    cli_print(ch, "%s", ch->prompt);
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
	char *tok = strtok(args, " \t");

	// If tok is not null, we have found an argument. If the argument is valid,
	// set a flag. If invalid, print an error message and return.
	while (tok != NULL) {
 		if (strcmp(tok, "--info") == 0) {
 			print_info = true;
 		} else if (strcmp(tok, "--help") == 0) {
 			print_help = true;
 		} else {
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
	its own help string. Otherwise, if we pass it the name of a command, it
	calls this procedure with the "--help" option.
*/
void cli_help(cli_chan_type *ch, char *args) {
	bool print_info = false;
	bool print_help = false;

	char *tok = strtok(args, " \t");
	
	while (tok != NULL) {
 		if (strcmp(tok, "--info") == 0) {
 			print_info = true;
 			break;
 		} else if (strcmp(tok, "--help") == 0) {
 			print_help = true;
 			break;
 		} else {
 			cli_cmd_proc proc = NULL;
			proc = cli_cmd_find(tok);
			if (proc != NULL) {
				proc(ch, "--help");
			} else {
				cli_print(ch, "ERROR: Unknown command '%s' in %s.\n", tok, __func__);
			}
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
"  help [command] [--info] [--help]\n"
"\n"
"Summary:\n"
"  List all registered commands and show their '--info' descriptions. If we pass\n"
"  the name of a command, the help command will call this named command with the\n"
"  '--help' option and so print help for the named command.\n"
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

/*
	cli_status accepts the required --info and --help arguments, but if we pass
	neither of these, it prints out system status and configuration report.
*/
void cli_status(cli_chan_type *ch, char *args) {
	bool print_info = false;
	bool print_help = false;
	char *tok = strtok(args, " \t");

	while (tok != NULL) {
 		if (strcmp(tok, "--info") == 0) {
 			print_info = true;
        } else if (strcmp(tok, "--help") == 0) {
        	print_help = true;
        } else {
            cli_print(ch, "ERROR: Unrecognized option '%s' in %s.\n",
            	tok, __func__);
            return;
        }
        tok = strtok(NULL, " \t");
    }
    
	if (print_info) {
		cli_message(ch, "Print table of system information.\n");
		return;
	}

	if (print_help) {
		cli_message(ch,
"Usage:\n"
"  status [--info] [--help]\n"
"\n"
"Summary:\n"
"  Print a table of system information. This includes network status, software tick\n"
"  frequency, microprocessor clock frequency, login status of the command-line\n"
"  interpreter, motherboard name, platform name, and software version number.\n"
"\n"
"Options:\n"
"  --info        Print a one-line summary of this command.\n"
"  --help        Print this help text.\n"
		);
		return;
	}

	cli_print(ch, "Motherboard     %s\n", EEM_MOTHERBOARD_NAME);
	cli_print(ch, "Module          %s\n", EEM_MODULE_NAME);
	cli_print(ch, "Software        %d\n", EEM_SOFTWARE_VERSION);
	server_info(ch->tx_buff);
	cli_print(ch, "%s\n", ch->tx_buff);
 	pic_info(ch->tx_buff);
	cli_print(ch, "%s\n", ch->tx_buff);
	switch (ch->status) {
		case CLI_CONNECTED: 
			strcpy(ch->tx_buff, "LOGIN_NONE"); 
			break;
			
		case CLI_PASS:
			strcpy(ch->tx_buff, "LOGIN_PASS"); 
			break;
			
		case CLI_FAIL: 
			strcpy(ch->tx_buff, "LOGIN_FAIL"); 
			break;
			
		case CLI_FAULT: 
			strcpy(ch->tx_buff, "FAULT"); 
			break;
			
		case CLI_CLOSE: 
			strcpy(ch->tx_buff, "CLOSE"); 
			break;
			
		default: 
			strcpy(ch->tx_buff, "UNKNOWN"); 
			break;
	}
	cli_print(ch, "ChannelStatus   %s\n", ch->tx_buff);
	cli_print(ch, "SecurityLevel   %d\n", eem_config_active.seclevel);
	if (debug) {
		cli_print(ch, "Debug           ON\n");
	} else {
		cli_print(ch, "Debug           OFF\n");	
	}

    return;
}

/*
	cli_login allows the user to attempt to log in to the EEM by sending the
	password. If the password matches the one in the active configuration, this
	routine sets the channel's logged in flag true. Otherwise it sets the logged
	in flag false. We must register this command under the name CLI_LOGIN_CMD.
*/
void cli_login(cli_chan_type *ch, char *args) {
	bool print_info = false;
	bool print_help = false;
	bool password_match = false;

	char *tok = strtok(args, " \t");
	while (tok != NULL) {
 		if (strcmp(tok, "--info") == 0) {
 			print_info = true;
 		} else if (strcmp(tok, "--help") == 0) {
 			print_help = true;
 		} else if (strcmp(tok, eem_config_active.password) == 0) {
			password_match = true;
			break;
        } else {
        	password_match = false;
        	break;
        }
        tok = strtok(NULL, " \t");
    }
    
	if (print_info) {
		cli_message(ch, "Sets login flag if password correct.\n");
		return;
	}

	if (print_help) {
		cli_message(ch,
"Usage:\n");
		cli_print(ch,
"  %s [password] [--info] [--help]\n", CLI_LOGIN_CMD);
		cli_message(ch,
"\n"
"Summary:\n"
"  Accepts a password string, which it compares to the password in the EEM's active\n"
"  configuration. If they match, the command sets this channels status to logged-in,\n"
"  and displays the current security level. If the security level is 0, loggin in\n"
"  makes no difference. At level 1, we must log in to display or modify EEM config-\n"
"  uration ields. At level 2, we must log in before we do anything other than log in.\n"
"  If the password does not match, login sets the channel status to a login failure\n"
"  code. Depending upon how the communication channel is set up, this code may cause\n"
"  immediate closure of the channel.\n"
"\n"
"Options:\n"
"  --info        Print a one-line summary of this command.\n"
"  --help        Print this help text.\n"
		);
		return;
	}

	if (password_match) {
		cli_print(ch, "Login succeeded, security level %d in %s.\n",
			eem_config_active.seclevel, __func__);
		ch->status = CLI_PASS;
	} else {
		cli_print(ch, "Login failed, security level %d in %s.\n",
			eem_config_active.seclevel, __func__);
		ch->status = CLI_FAIL;
	}

    return;
}

/*
	cli_exit sets the channel state to CLI_CLOSE. The channel management routine
	can act upon this status as it sees fit. In the case of a TCP socket,
	recommend the task routine return an error code to tcpip_server so as to
	close the socket. In the case of the UART serial channel, we recomment the
	console server ignores the channel state.
*/
void cli_exit(cli_chan_type *ch, char *args) {
	bool print_info = false;
	bool print_help = false;

	char *tok = strtok(args, " \t");
	while (tok != NULL) {
 		if (strcmp(tok, "--info") == 0) {
 			print_info = true;
 		} else if (strcmp(tok, "--help") == 0) {
 			print_help = true;
 		} else {
            cli_print(ch, "ERROR: Unrecognized option '%s' in %s.\n", tok, __func__);
            return;
        }
        tok = strtok(NULL, " \t");
    }
    
	if (print_info) {
		cli_message(ch, "Attempts to close the communication channel.\n");
		return;
	}

	if (print_help) {
		cli_message(ch,
"Usage:\n"
"  exit [--info|--help]\n"
"\n"
"Summary:\n"
"  Requests that the communication channel manager close the channel. In the\n"
"  case of a TCP socket, the socket should close. In the case of a UART serial\n"
"   interface, nothing will happen.\n"
"\n"
"Options:\n"
"  --info        Print a one-line summary of this command.\n"
"  --help        Print this help text.\n"
		);
		return;
	}

	ch->status = CLI_CLOSE;

    return;
}

/*
	cli_prompt allows the CLI user to set the prompt string. We enclose the prompet in
	double-quotes, and the prompt can be an empty string.
*/
void cli_prompt(cli_chan_type *ch, char *args) {
	bool print_info = false;
	bool print_help = false;
	bool prompt_received = false;
	char *prompt;

	char *tok = strtok(args, "\"");
	if (tok != NULL) {
		prompt_received = true;
		prompt = tok;
		if (strcmp(tok, "--info") == 0) {
			print_info = true;
		}
		if (strcmp(tok, "--help") == 0) {
			print_help = true;
		}
	} 

	if (print_info) {
		cli_message(ch, "Sets the command-line interpreter prompt.\n");
		return;
	}

	if (print_help) {
		cli_message(ch,
"Usage:\n"
"  prompt [--info|--help]\n"
"  prompt \"prompt\"\n"
"\n"
"Summary:\n"
"  Sets the Command-Line Interpreter (CLI) prompt. We specify a string to act as\n"
"  the prompt. If the string contains spaces, we must enclose the string in double\n"
"  -quotes. The string can be empty, which is a good choice for automated\n"
"  communication with the interpreter. If we pass no string in double-quotes and\n"
"  neither of the reporting options, the routine will set the prompt to an empty\n"
"  string.\n"
"\n"
"Options:\n"
"  --info        Print a one-line summary of this command.\n"
"  --help        Print this help text.\n"
		);
		return;
	}

	if (prompt_received) {
		strcpy(ch->prompt, prompt);
	} else {
		strcpy(ch->prompt, "");
	}

    return;
}


