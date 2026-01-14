/*
	console.h -- Interface of the Console library. The console is a hard-wired 
	UART connection to the EEM. Under normal operating conditions, the console
	will not be connected to anything. The console UART will transmit to nothing
	and there will be no incoming characters. But when we are trying to track
	down bugs in our EEM code, the console is useful because it provides a
	connction to the EEM that does not require the TCP/IP stack and Ethernet
	physical interface to be up and running. 
	
	The EEM uses UART2 for its console interface. In addition to error and debug
	reporting, the console provides a command-line interpreter (CLI). If our
	console prints are frequent, they will disrupt our efforts to compose
	commands for the CLI. But the commands will still be read by the CLI and
	executed. 

	The console library provides a set of routines for writing to the console
	interface. Most of these print strings, but we can also print binary values,
	such as four-byte integers so that we can view them on an oscilloscope as
	they are transmitted. If we do not have a UART-to-USB bridge connected from
	the EEM to our computer, binary output with oscilloscope view is our
	most basic means of obtaining reports from our code.

	In addition to its own printing routines, the console library loads a
	modified version Microchip library's system console header file and then
	provides definitions for the Microchip SYS_CONSOLE message and print
	routines. These routines are used by some Microchip libraries, and are
	expected by others. In our EEM code, we see the TCP/IP stack calling a
	console print to declare that is it initializing and then running, but no
	other use of the SYS_CONSOLE routines is manifested.
	
	The UART console is the default destination of all console messages, be they
	error messages, progress reporting, or debug diagnostic messages. But we can
	divert all console messages to a TCP socket if we want to, by setting the
	global console_output pointer to point to a TCP socket index. Now all these
	messages will be written to the selected socket instead.

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

#ifndef CONSOLE_H
#define CONSOLE_H

/*
	Include the Microchip SYS_CONSOLE routine declarations. We will be supplying 
	functions to perform the functions of these routines, so that the Microchip
	libraries can write to our UART serial console.
*/
#include "microchip/system/console/sys_console.h"

/*
	We need the CLI declarations for the CLI channel types. These are used by the
	command procedures we declare below.
*/
#include "cli.h"

/*
	Procedures that manage and configure console text reporting.
*/
void console_message(const char *s);
void console_print(const char *fmt, ...);
void console_initialize(void);
extern void *console_context;

/*
	Console commands for the command-line interpreter (CLI).
*/
void cli_debug(cli_chan_type *ch, char *args);

/*
	Hardware debugging console routines that write directly to the UART2 and only
	to the UART2.
*/
void console_put_int_hex(uint32_t value);
void console_put_int_trace(uint32_t value);
void console_write(const void *buff, size_t size);

#endif
