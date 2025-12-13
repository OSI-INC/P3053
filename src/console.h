/*
	console.h -- Interface of the Process Reporting Console library. The primary
	purpose of the console is to display diagnostic messages to help us debug
	our Embedded Ethernet Module (EEM) code and measure its performance. The EEM
	directs console output to the PIC32MZ's UART2 interface. The console is
	output-only: it does not seek input from the interface. Its output routines
	block only so long as it takes them to write characters into the UART, which
	is a bounded and finite time, so long as the UART is still running. We can
	use the UART2 interface for other purposes, such as a command line interface
	(CLI). If our console prints will interleave with the CLI command responses.
	If our console prints are frequent, they will disrupt our efforts to compose
	commands for the CLI. But the commands will still be read by the CLI and
	executed.

	The console library provides a set of routines for writing to the console
	interface. Most of these print strings, but we can also print binary values,
	such as four-byte integers, so that we can view them on an oscilloscope as
	they are transmitted. If we do not have a UART-to-USB bridge connected from
	the EEM to our computer, this binary output with oscilloscope view is our
	most basic means of obtaining reports from our code.

	When we compile our code with "make debug", the VERBOSE_CONSOLE flag will be
	set. Our console library uses this flag to set the more succinct, global
	"debug" flag, which we can use in our routines to make console reporting
	contingent upon a debug build, as in "if (debug) console_message(string)".
	The "debug" name does not appear to be used in any of our Harmony libraries.

	In addition to its own printing routines, the console library loads a
	modified version Harmony system console header file, and then provides
	definitions for the Harmony SYS_CONSOLE family of read and write routines as
	well. These routines are used by some Harmony libraries, and are expected by
	others. In our EEM code, we see only the TCP/IP stack calling a console
	print to declare that is it initializing and then running.

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
	will replace the console with the generic and multi-channel command line
	interpreter routines.
*/
void console_putchar(char c);
void console_flush(void);
int console_readcount(void);
int console_getchar(void);
void console_put_int_hex(uint32_t value);
void console_put_int_trace(uint32_t value);
void console_write(const void* buff, size_t size);
void console_message(const char *s);
void console_print(const char* fmt, ...);
void console_dump_hex(const char* s);
void console_dump_ascii(const char* s);

/*
	The console initialization and service routines.
*/
void console_initialize(void);
void console_server(void);

#endif
