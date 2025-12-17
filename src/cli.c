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
	cli_help with no arguments prints to the specified channel a list of all
	registered commands and their information strings. If we pass it "--info" it
	prints its info string only. Otherwise, if we pass it "--help", it prints
	its own help string.
*/
void cli_help(cli_chan_type *ch, char *args) {

	// Option flags.
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
	// channel.
	if (print_info) {
		cli_message(ch, "List all commands with descriptions.\n");
		return;
	}

	// The --help option must be supported by all CLI commands. It prints a
	// longer help message that acts as a manual page for the command, listing
	// all options and their functions.
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

	// No options were given, so perform the default function: print the full
	// CLI command help list.
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
	cli_reset performs a software reset of the Embedded Ethernet Module (EEM). 
*/
void cli_reset(cli_chan_type *ch, char *args) {
	bool print_info = false;
	bool print_help = false;
	char* tok = strtok(args, " \t");

	while (tok != NULL) {
 		if (strcmp(tok, "--info") == 0) {print_info = true;} 
 		else if (strcmp(tok, "--help") == 0) {print_help = true;} 
 		else {
            cli_print(ch, "ERROR: Unrecognized option '%s' in %s.\n", tok, __func__);
            return;
        }
        tok = strtok(NULL, " \t");
    }
    
	if (print_info) {
		cli_message(ch, "Initiate a software reset of the EEM.\n");
		return;
	}

	if (print_help) {
		cli_message(ch,
"Usage:\n"
"  reset [--info] [--help]\n"
"\n"
"Summary:\n"
"  Performs a software reset of the Embedded Ethernet Module (EEM). With no options,\n"
"  the command prints a reset notification. It waits for a quarter-second before\n"
"  initiating the reset.\n"
"\n"
"Options:\n"
"  --info        Print a one-line summary of this command.\n"
"  --help        Print this help text.\n"
		);
		return;
	}
	
	cli_message(ch, "Resetting the EEM...\n");
	int counter = 1e6;
	while (counter > 0) {
		tcpip_tick();
		counter--;
	}
	pic_reset();
    return;
}

/*
	cli_ip_config with no arguments prints to the specified channel a list of
	network information. With the --ip, --gateway, or --mask options, it changes
	the current server values and copies the new values into the active 
	Embedded Ethernet Module (EEM) configuration. 
*/
void cli_ip_config(cli_chan_type *ch, char *args) {
	bool print_info = false;
	bool print_help = false;
	bool update = false;
	char ip_str[32];
	char gw_str[32];
	char nm_str[32];
	uint32_t lp;
	uint32_t tp;
	
	server_ip_str(ip_str);
	server_gw_str(gw_str);
	server_nm_str(nm_str);
	tp = eem_config_active.telnet_port;
	lp = eem_config_active.lwdaq_port;
	
	char* tok = strtok(args, " \t");
	while (tok != NULL) {
		if (strcmp(tok, "--ip") == 0) {
			strcpy(ip_str, strtok(NULL, " \t"));
			update = true;
		} else if (strcmp(tok, "--gateway") == 0) {
			strcpy(gw_str, strtok(NULL, " \t"));
			update = true;
		} else if (strcmp(tok, "--mask") == 0) {
			strcpy(nm_str, strtok(NULL, " \t"));
			update = true;
		}  else if (strcmp(tok, "--telnet-port") == 0 
				|| strcmp(tok, "--lwdaq-port") == 0) {
			char* val = strtok(NULL, " \t");
			if (!val) {
    			cli_print(ch, 
    				"ERROR: Option '%s' requires value in %s.\n", 
    				tok, __func__);
        		return;
			}
			char *end;
    		unsigned long p = strtoul(val, &end, 10);
			if (*end != '\0') {
				cli_print(ch,
					"ERROR: Value '%s' invalid for %s in %s.\n", 
					val, tok, __func__);
				return;
			}
			if (p == 0 || p > 65535) {
				cli_print(ch,
					"ERROR: Value '%lu' out of range (1-65535) for %s in %s.\n", 
					p, tok, __func__);
				return;
			}
			if (strcmp(tok, "--telnet-port") == 0) {
				tp = (uint32_t) p;
			} else {
				lp = (uint32_t) p;
			}
			update = true;
		} else if (strcmp(tok, "--info") == 0) {
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
		cli_message(ch, "List internet protocol network configuration.\n");
		return;
	}

	if (print_help) {
		cli_message(ch,
"Usage:\n"
"  ip-config [OPTIONS]\n"
"\n"
"Summary:\n"
"  Change network configuration as specified by options, then display the network\n" 
"  configuration. With no options, no changes will be made, but network information.\n"
"  will still be displayed.\n"
"\n"
"Options:\n"
"  --ip <addr>           Set IP address to new string 'x.x.x.x'.\n"
"  --gateway <addr>      Set gateway address to new string 'x.x.x.x'.\n"
"  --mask <addr>         Set network mask to new string 'x.x.x.x'.\n"
"  --telnet-port <port>  Set the port used for the Telnet server.\n"
"  --lwdaq-port <port>   Set the port used for the LWDAQ server.\n"
"\n"
"  --info                Display a one-line summary of this command. When specified,\n"
"                        no configuration changes are made.\n"
"  --help                Display detailed help for this command. When specified, no\n"
"                        configuration changes are made.\n"
		);
		return;
	}
	
	if (update) {
		eem_config_active.telnet_port = tp;
		eem_config_active.lwdaq_port = lp;
		if (server_set_ip(ip_str, gw_str, nm_str) >= 0) {
			server_ip_str(eem_config_active.ip_str);
			server_gw_str(eem_config_active.gw_str);
			server_nm_str(eem_config_active.nm_str);
		} else {
			cli_print(ch,
				"ERROR: Failed to update IP interface in %s.\n", __func__);
		}
	}
	
	server_info(ch->tx_buff);
	cli_print(ch, "%s\n", ch->tx_buff);

    return;
}

/*
	cli_pic_info with no arguments prints PIC32MZ characteristics. It does
	not offer any way to change those characteristics.
*/
void cli_pic_info(cli_chan_type *ch, char *args) {
	bool print_info = false;
	bool print_help = false;
	char* tok = strtok(args, " \t");

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
		cli_message(ch, "List PIC32MZ internal information.\n");
		return;
	}

	if (print_help) {
		cli_message(ch,
"Usage:\n"
"  pic-info [--info] [--help]\n"
"\n"
"Summary:\n"
"  List internal PIC32MZ microcontroller configuration values. Provides no means to\n"
"  hange any of these values.\n"
"\n"
"Options:\n"
"  --info        Print a one-line summary of this command.\n"
"  --help        Print this help text.\n"
		);
		return;
	}

 	pic_info(ch->tx_buff);
	cli_print(ch, "%s\n", ch->tx_buff);

    return;
}

/*
	cli_eem_config shows or sets Embedded Ethernet Module (EEM) configuration
	records. We specify which we want to do: show or set. For "show", we name a
	configuration to show, and there are three to choose from: active, flash, or
	factory. For "set", we name a configuration to set and there is only one we
	can set: the one in flash, which will be deployed on the next start-up. We
	cannot set the factory configuration because it is read-only, and we cannot
	set the active configuration because any changes to the active configuration
	would cause the active configuration to be inaccurate.
*/
void cli_eem_config(cli_chan_type *ch, char *args) {
	enum {
		CFG_NONE,
		CFG_ACTIVE,
		CFG_FLASH,
		CFG_FACTORY
	};
	int destination = CFG_NONE;
	int source = CFG_NONE;
    bool do_set = false;
    bool print_info = false;
    bool print_help = false;
	
	char* tok = strtok(args, " \t");
	while (tok != NULL) {
		if (strcmp(tok, "--info") == 0) {
			print_info = true;
		} else if (strcmp(tok, "--help") == 0) {
			print_help = true;
		} else if (strcmp(tok, "show") == 0) {
			do_set = false;
		} else if (strcmp(tok, "set") == 0) {
			do_set = true;
		} else if (strcmp(tok, "active") == 0) {
			if (!do_set) {
				destination = CFG_ACTIVE;
			} else if (destination == CFG_NONE) {
				cli_print(ch,"ERROR: Cannot set active configuration in %s.\n",
					__func__);
				return;
			} else if (source == CFG_NONE) {
				source = CFG_ACTIVE;
			} else {
				cli_print(ch,"ERROR: Too many configuration selectors in %s.\n",
					__func__);
				return;
			}		
    	} else if (strcmp(tok, "flash") == 0) {
			if (!do_set) {
				destination = CFG_FLASH;
			} else if (destination == CFG_NONE) {
				destination = CFG_FLASH;
			} else if (source == CFG_NONE) {
				source = CFG_FLASH;
			} else {
				cli_print(ch,"ERROR: Too many configuration selectors in %s.\n",
					__func__);
				return;
			}
		} else if (strcmp(tok, "factory") == 0) {
			if (!do_set) {
				destination = CFG_FACTORY;
			} else if (destination == CFG_NONE) {
				cli_print(ch,"ERROR: Cannot set factory configuration in %s.\n",
					__func__);
				return;
			} else if (source == CFG_NONE) {
				source = CFG_FACTORY;
			} else {
				cli_print(ch,"ERROR: Too many configuration selectors in %s.\n",
					__func__);
				return;
			}
		} else {
			cli_print(ch, "ERROR: Unrecognized option '%s' in %s.\n",
				tok, __func__);
			return;
		}
		tok = strtok(NULL, " \t");
	}
	
	if (do_set && destination == CFG_NONE) {
		cli_print(ch, "ERROR: Set requires destination selector in %s.\n",
			__func__);
		return;
	}

	if (print_info) {
		cli_message(ch, "Display or modify EEM configuration records.\n");
		return;
	}

	if (print_help) {
		cli_message(ch,
"Usage:\n"
"  eem-config [show] [active|flash|factory]\n"
"  eem-config set flash [active|flash|factory]\n"
"\n"
"Notes:\n"
"  - If no verb is specified, 'show' is assumed.\n"
"  - If no selector is specified, 'active' is assumed.\n"
"  - For 'set', the destination must be specified.\n"
"  - For 'set', the source defaults to 'active'.\n"
"  - For 'set', destination must be 'flash'.\n"
"\n"
"Description:\n"
"  Display or modify Embedded Ethernet Module (EEM) configuration records. If no\n"
"  verb is specified, the 'show' operation is performed on the active configuration.\n"
"  The flash configuration is the only one the 'set' operation can modify. The three\n"
"  available configuration records are: active, flash, and factory. We can show any\n"
"  one, but we can set only the flash record. The active configuration represents\n"
"  the current state of the EEM. The flash configuration is the one that resides in\n"
"  flash memory. The factory configuration is a read-only record that will be loaded\n"
"  when the EEM boots up and sees that its host's configuration switch is depressed\n"
"  The configuration switch initiates a 'factory reset'. The EEM will copy the\n"
"  factory configuration to the flash configuration, load the flash configuration\n"
"  into the active configuration, and implement the active configuration. Duruing a\n"
"  normal start-up, in wich the configuration switch on the host is not depressed,\n"
"  the EEM loads the flash configuration directly into the active configuration and\n"
"  implements the active configuraiton. By this means we provide both persistent EEM\n"
"  configuration through reset and power cycles, and also the means to restore the\n"
"  EEM to a known state.\n"
"\n"
"Verbs:\n"
"  show       Display configuration records (default).\n"
"  set        Modify a configuration record.\n"
"\n"
"Selectors:\n"
"  active     Currently active configuration.\n"
"  flash      Configuration stored in flash memory.\n"
"  factory    Factory-default configuration (read-only).\n"
"\n"
"Options:\n"
"  --info     Display a one-line summary.\n"
"  --help     Display this help text.\n"
		);
		return;
	}
	
    if (!do_set) {
    	if (destination == CFG_NONE) source = CFG_ACTIVE;
 		switch (destination) {
			case CFG_ACTIVE: {
				server_str_from_config(ch->tx_buff, &eem_config_active);
			}
			break;
			
			case CFG_FLASH: {
				eem_config_type config;
				eem_config_read(&config); 
				server_str_from_config(ch->tx_buff, &config);
			}
			break;

			case CFG_FACTORY: {
				server_str_from_config(ch->tx_buff, &eem_config_factory);
			}
			break;
		}
		cli_print(ch, "%s\n", ch->tx_buff);
    } else {
    	if (source == CFG_NONE) source = CFG_ACTIVE;
		if (source == destination) {
			cli_print(ch, "Configuration unchanged, source = destination in %s.\n",
				__func__);
			return;
		}
		if (destination == CFG_FLASH) {
			if (source == CFG_FACTORY) {
				eem_config_write(&eem_config_factory);
				console_print(
					"Copied factory configuration to flash in %s.\n", __func__);
			}
			if (source == CFG_ACTIVE) {
				eem_config_write(&eem_config_active);
				console_print(
					"Copied active configuration to flash in %s.\n", __func__);
			}
		}
		if (destination == CFG_ACTIVE) {
			if (source == CFG_FACTORY) {
				eem_config_active = eem_config_factory;
				console_print(
					"Copied factory configuration to active in %s.\n", __func__);
			}
			if (source == CFG_FLASH) {
				eem_config_read(&eem_config_active);
				console_print(
					"Copied flash configuration to active in %s.\n", __func__);
			}
		}
	}
	
    return;
}

/*
	cli_initialize initializes the command-line interpreter by registering some
	commands we find useful. We can add more commands at any time with the
	cli_cmd_register routine.
*/
void cli_initialize(void) {
	cli_cmd_register("help",cli_help);
	cli_cmd_register("reset",cli_reset);
	cli_cmd_register("eem-config",cli_eem_config);
	cli_cmd_register("ip-config",cli_ip_config);
	cli_cmd_register("pic-info",cli_pic_info);
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
	const char* prompt = "EEM$ ";

	// Static buffers are persistent, but we do not use them to carry
	// information from one execution of this routine to the next. We merely
	// avoiding re-allocating stack space for these buffers every time we call
	// the server routine. Note that these lengths include the null character we
	// put at the end of the command name or argument list.
    static char cmd[CLI_CMD_SIZE];
    static char args[CLI_ARGS_SIZE];
	

	// Read all available bytes into the channel's receive buffer. We execute
	// backspace and delete immediately we see them. If echo is enabled, we echo
	// all characters, with backspace and delete being echoed as "backspace
	// space backspace" in order to remove the most-recently echoed character
	// from view.
    while (ch->rx_len < CLI_RX_SIZE - 1) {
    	int c = ch->getchar(ch->context);
    	if (c < 0) {
    		break;
		} else if (c == '\b' || c == 0x7F) {
			if (ch->rx_len > 0) {
				ch->rx_len--;
				ch->rx_buff[ch->rx_len] = '\0';
			}
			if (ch->echo) {
				cli_message(ch->context, "\nBACKSPACE");
			}
		} else {
			ch->rx_buff[ch->rx_len] = (char) c;
			ch->rx_len++;
			ch->rx_buff[ch->rx_len] = '\0';
			if (ch->echo) ch->putchar(ch->context, c);
		}
    }

	// If we have no characters at all, return now.
	if (ch->rx_len == 0) return;

	// If the buffer has overflowed, reset it and report an error.
	if (ch->rx_len >= CLI_RX_SIZE) {
		cli_print(ch,"ERROR: Receive buffer overflow, "
			"discarding all characters in %s.\n", ch->name);
		ch->rx_len = 0;
		ch->rx_buff[0] = '\0';
		return;
	}
	
	// Look for a newline in the characters we have available. If we find one,
	// record its location. If we don't find one, return.
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
    if (ch->rx_buff[eol] == '\r' 
    		&& eol < ch->rx_len - 1 
    		&& ch->rx_buff[eol+1] == '\n') {
        consume = 2;
    } else if (ch->rx_buff[eol] == '\n' 
    		&& eol < ch->rx_len - 1 
    		&& ch->rx_buff[eol+1] == '\r') {
        consume = 2;
    }
    
    // Overwrite the first newline character with a null to make a null-terminated
    // line string.
    ch->rx_buff[eol] = '\0';

	// Now we normalize the line by executing backspace and delete characters. We
	// make sure we treat characters we read from the receive buffer as unsigned
	// values so that we can compare them to ASCII decimal values. Each time we read
	// a printable character, we add it to the normalized string. Backspaces and
	// deletes should have been implemented and eliminated from the string by now,
	// but we look out for them anyway. We add a new null terminator at the end.
    char *src = ch->rx_buff;
    char *dst = ch->rx_buff;
    while (*src != '\0') {
		unsigned char c = (unsigned char)* src++;
		if (c == '\b' || c == 0x7F) {
			if (dst > ch->rx_buff) dst--;
			continue;
		}
		if (c < 32 || c == 127) continue;
		*dst++ = (char) c;
    }
    *dst = '\0';

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
	
	// Shift any remaining characters all the way to the start of the receive
	// buffer. We will deal with them the next time we call this routine.
	uint32_t shift = eol + consume;
	uint32_t remain = ch->rx_len - shift;
	if (remain > 0) memmove(ch->rx_buff, &ch->rx_buff[shift], remain);
	ch->rx_len = remain;
	ch->rx_buff[ch->rx_len] = '\0';
	
	// If the command name is an empty string, print the prompt.
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

    // Print the prompt.
    cli_message(ch, prompt);
}

