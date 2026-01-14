/*
	console.c -- Implementation of the Console library.

	Copyright (C) 2025-2026, Kevan Hashemi, Open Source Instruments Inc.

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
#include "console.h"

/*
	The console_context pointer is initialized to NULL. So long as it remains NULL,
	all console messages will be printed to the UART2 console interface. But if
	if points to a TCP socket, the messages will be printed to the socket instead.
*/
void *console_context = NULL;

/*
	console_putchar writes a character to the UART2 or to a TCP socket,
	depending upon the value of the global console context pointer. If the
	context pointer is set, the routine checks to see if it points to a valid
	socket. If not, the routine clears the pointer. If the context pointer is
	cleared, the routine writes to the serial interface. Otherwise it writes to
	the specified socket. This algorithm for checking whether the socket is
	valid or not does not necessarily mean that the console messages will be
	restored to the serial port once a socket that redirected the debug
	reporting to itself has been closed. If no call to console_putchar takes
	place before another client is connected, this new client may be assigned
	the same socket record as was used by the redirecting socket. Now console
	messsages will be directed to the new socket.
*/
void console_putchar(char c) {
	if (console_context != NULL) {
		TCP_SOCKET sock = *(TCP_SOCKET *) console_context;
		if (sock == INVALID_SOCKET) {
			console_context = NULL;
		}
	} 
	if (console_context == NULL) {
		uart2_putchar(NULL, c);
	} else {
		tcp_putchar(console_context, c);
	}
}

/*
	console_message writes an entire null-terminated string of characters to the
	console, and possibly to a tcp socket if one has been assigned to be a
	secondary destination for console messages. We pass the routine a pointer to
	a string, and it keeps reading characters from the string and writing them
	to the console until it reaches a null character, which it does not write.
	One additional service this routine provides is to handle different
	protocols for carriage returns and line feeds. The routine always prints
	CRLF ("\r\n") for a new line. If it sees just CR on its own, or LF on its
	own, it prints CRLF. But if it sees CRLF, it prints only one CRLF, not two
	CRLFs.
*/
void console_message(const char *s) {
    bool expect_lf = false;

    while (*s) {
        if (*s == '\r') {
            console_putchar('\r');
            console_putchar('\n');
            expect_lf = true;
        } else if (*s == '\n') {
            if (!expect_lf) {
                console_putchar('\r');
                console_putchar('\n');
            }
            expect_lf = false;
        } else {
            console_putchar(*s);
            expect_lf = false;
        }
        s++;
    }
}

/*
	console_print converts a string with conversion specifiers and a list of
	literal arguments, converts it into a literal string, and prints the literal
	string to the console, and possibly a tcp socket if one has been assigned as
	a secondary destiniation for console messages. For each literal argument
	there must be a conversion specification in the string. The conversion
	specifier begins with a percent symbol. Simple examples are: %d for signed
	integer, %u for unsigned integer,
	%x for hexadecimal, %s for a string, %c for a character, %f for a floating
	point number or or a double-length floading point number. In the case of the
	"f" format specifier, we can have %10.3f for width ten characters, padded
	with spaces on the left, and three digits after the decimal point. The
	routine uses the machinery provided by stdarg.h to go through the format
	specficiations and literal arguments. This machinery is: va_start, va_list,
	vsnprintf, and va_end.
*/
void console_print(const char *fmt, ...) {
    char buff[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buff, sizeof(buff), fmt, args);
    va_end(args);
    console_message(buff);
}

/*
	SYS_CONSOLE_Message is a routine used by Microchip system processes. It
	prints a literal string to the console. Its first argument is an index that
	selects the console to be printed to, but we have only one console, so we
	ignore the index. The next argument is a pointer to a null-terminated
	string. We pas that pointer to our own console_message routine.
*/
void SYS_CONSOLE_Message(int index, const char *msg) {
    (void) index;
    console_message(msg);
}

/*
	SYS_CONSOLE_Print is a routine used by the Microchip system processes. It
	prints a string with formatted contents to the console. Its first argument
	is an index that selects a console to be printed to, but we have only one
	console, so we ignore the index. The next argument is a pointer to a
	nul-terminated string. After that are zero or more literal values that will
	be formatted by conversion specifications within the string. Because of the
	variable number of arguments, we cannot simply call console_print and pass
	it the string and arguments. We must repeat our console_print instructions,
	using the same stdarg.h machinery.
*/
void SYS_CONSOLE_Print(int index, const char *fmt, ...) {
	(void) index;
    char buff[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buff, sizeof(buff), fmt, args);
    va_end(args);
    console_message(buff);
}

/*
	console_initialize sets up the UART2 as our console interface. The UART2 will
	receive and transmit characters from a UART terminal. The routine immediately
	prints an announcement, even if the REPORT flag is not set.
*/
void console_initialize(void)
{
	UART2_Initialize();
	UART_SERIAL_SETUP uart2Setup = {
		.baudRate = 115200,
		.dataWidth = UART_DATA_8_BIT,
		.parity = UART_PARITY_NONE,
		.stopBits = UART_STOP_1_BIT
	};
	UART2_SerialSetup(&uart2Setup, 0);

	console_print("\n\n");
	console_print("===========================================================\n");
	console_print("=========    Embedded Ethernet Module Console     =========\n");
	console_print("===========================================================\n");
	if (debug) {
		console_print("The debug flag is set, expect verbose output.\n");
	} else {
		console_print("The debug flag is not set, expect quiet output.\n");	
	}
}

/*
	cli_debug is a Command-Line Interpreter (CLI) command that configures debug
	printing. We can set or clear the global debug flag. We can direct debug
	printing to the current CLI communication channel, or we can restore debug
	printing to the UART console.
*/
void cli_debug(cli_chan_type *ch, char *args) {
	bool print_info = false;
	bool print_help = false;
	bool verb_received = false;
	bool set_flag = false;
	bool clear_flag = false;
	bool grab_flag = false;
	bool release_flag = false;

	char *tok = strtok(args, " \t");
	while (tok != NULL) {
 		if (strcmp(tok, "--info") == 0) {
 			print_info = true;
 		} else if (strcmp(tok, "--help") == 0) {
 			print_help = true;
 		} else if (strcmp(tok, "on") == 0 
 				|| strcmp(tok, "off") == 0 
 				|| strcmp(tok, "grab") == 0 
 				|| strcmp(tok, "release") == 0) {
 			if (verb_received) {
				cli_print(ch,"ERROR: Only one command verb permitted in %s.\n",
					__func__);
				return;			
 			}
 			if (strcmp(tok, "on") == 0) set_flag = true;
 			if (strcmp(tok, "off") == 0) clear_flag = true;
 			if (strcmp(tok, "grab") == 0) grab_flag = true;
 			if (strcmp(tok, "release") == 0) release_flag = true;
 			verb_received = true;
 		} else {
            cli_print(ch, "ERROR: Unrecognized option '%s' in %s.\n", tok, __func__);
            return;
        }
        tok = strtok(NULL, " \t");
    }
    
	if (print_info) {
		cli_message(ch, "Sets or clears the debug flag.\n");
		return;
	}

	if (print_help) {
		cli_message(ch,
"Usage:\n"
"  debug [on|off|grab|release] [--info] [--help]\n"
"\n"
"Summary:\n"
"  The 'on' and 'off' verbs set and clear the global debug flag respectively.\n"
"  The debug flag turns on and off all console print commands that are made\n"
"  conditional upon the state of the flag, which we intend to be all debugging\n"
"  messages. The initial state of this flag is set by a constant in the source code.\n"
"  The 'grab' command directs all console messages to the communication channel in\n"
"  which the grab command was entered. The 'release' command restores the console\n"
"  message to the serial interface, which is a hard-wired, three-wire interface on\n"
"  the EEM itself. If we redirect the console output to a socket, and the socket\n"
"  subsequently closes, the console messages will usually restore themselves to the\n"
"  serial interface, but might be directed towards a new socket that took the place\n"
"  of the socket that closed. If we pass no options to the routine, the command\n"
"  returns the state of the debug flag and the current destination of console prints.\n"
"  For the serial interface, the command writes 'UART2' and for a socket interface\n"
"  the command writes 'SOCKx', where 'x' is the socket index.\n"
"\n"
"Verbs:\n"
"  on           Set the debug flag, debug prints enabled.\n"
"  off          Clear the debug flag, debug prints disabled.\n"
"  grab         Redirect console reporting to this channel.\n"
"  release      Restore console reporting to the serial port.\n"
"\n"
"Options:\n"
"  --info        Print a one-line summary of this command.\n"
"  --help        Print this help text.\n"
		);
		return;
	}

	if (!verb_received) {
		if (debug) {
			cli_message(ch, "on ");
		} else {
			cli_message(ch, "off ");
		}
		if (console_context != NULL) {
			TCP_SOCKET sock = *(TCP_SOCKET *) console_context;
			if (sock == INVALID_SOCKET) {
				console_context = NULL;
				cli_message(ch, "UART2\n");
			} else {
				cli_print(ch, "SOCK%d\n", (int) sock);
			}
		} else {
			cli_message(ch, "UART2\n");
		}
	} else {
		if (set_flag) debug = true;
		if (clear_flag) debug = false;
		if (grab_flag) console_context = ch->context;
		if (release_flag) console_context = NULL;
	}
	
    return;
}

/*
	console_put_int_hex takes a thirty-two bit integer, which is eight nibbles,
	and prints it as eight hexadecimal ASCII digits, most significant nibble
	first, to the console. This routine does not provide support for a secondary
	destination, but prints only to UART2.
*/
void console_put_int_hex(uint32_t value) {
    const char hex[] = "0123456789ABCDEF";
    for (int shift = 28; shift >= 0; shift -= 4)
        uart2_putchar(NULL, hex[(value >> shift) & 0xF]);
}

/*
	console_put_int_trace uses the UART2 interface to display a thirty-two bit
	integer for viewing on an oscilloscope. It takes an integer value and
	transmits it, least significant byte first, over the UART. Because the UART
	sends the least significant bit first, we will see the bits on our
	oscilloscope trace of UART2 TX line as bit 0 to 32, with a stop bit (1) and
	a start bit (0) between each of the bytes. We can use it to broadcast raw
	register values, as in console_putint(RPF3R). We used this routine to look
	at LATF, PIRTF, ODCF, and RPF3R when we were investigating our start-up GPIP
	problems. By looking at the bits on an oscilloscope, we do not need a
	functioning UART-to-USB bridge, and we can view register bits toggling more
	easily.	This routine does not provide support for a secondary
	destination, but prints only to UART2.	
*/
void console_put_int_trace(uint32_t value) {
	int i;
	for (i = 0; i <= 3; i++) {
		uart2_putchar(NULL, ((uint8_t*)&value)[i]);
	}
}

/*
	console_write writes a raw byte array write to the console, with no regard
	to the byte values, including no recognition of a null character as a
	terminator. We pass it a pointer to the byte array and the size of the
	array. This routine does not provide support for a secondary destination,
	but prints only to UART2.
*/
void console_write(const void *buff, size_t size) {
    const uint8_t *p = (const uint8_t*) buff;
    for (size_t i = 0; i < size; i++) {
        uart2_putchar(NULL, p[i]);
    }
}
