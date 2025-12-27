/*
	console.c -- Implementation of the Process Reporting Console library.

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
	console_put_int_hex takes a thirty-two bit integer, which is eight nibbles,
	and prints it as eight hexadecimal digits, most significant nibble first, to
	the console.
*/
void console_put_int_hex(uint32_t value) {
    const char hex[] = "0123456789ABCDEF";
    for (int shift = 28; shift >= 0; shift -= 4)
        uart2_putchar(NULL, hex[(value >> shift) & 0xF]);
}

/*
	console_put_int_trace is not an ASCII-reporting routine. It is designed to
	allow us to view thirty-two bit integers on an oscilloscope attached to our
	console TX signel. It takes an integer value and transmits it, least
	significant byte first, over the UART. Because the UART sends the least
	significant bit first, we will see the bits on our oscilloscope trace of
	UART2 TX line as bit 0 to 32, with a stop bit (1) and a start bit (0)
	between each of the bytes. We can use it to broadcast raw register values,
	as in console_putint(RPF3R). We used this routine to look at LATF, PIRTF,
	ODCF, and RPF3R when we were investigating our start-up GPIP problems. By
	looking at the bits on an oscilloscope, we do not need a functioning
	UART-to-USB bridge, and we can view register bits toggling more
	easily.		
*/
void console_put_int_trace(uint32_t value) {
	int i;
	for (i = 0; i <= 3; i++) uart2_putchar(NULL, ((uint8_t*)&value)[i]);
}

/*
	console_write writes a raw byte array write to the console, with no regard
	to the byte values, including no recognition of a null character as a
	terminator. We pass it a pointer to the byte array and the size of the
	array.
*/
void console_write(const void* buff, size_t size) {
    const uint8_t* p = (const uint8_t*) buff;
    for (size_t i = 0; i < size; i++) {
        uart2_putchar(NULL, p[i]);
    }
}

/*
	console_message writes an entire null-terminated string of characters to the
	console. We pass the routine a pointer to a string, and it keeps reading
	characters from the string and writing them to the console until it reaches
	a null character, which it does not write. One additional service this
	routine provides is to handle different protocols for carriage returns and
	line feeds. The routine always prints CRLF ("\r\n") for a new line. If it
	sees just CR on its own, or LF on its own, it prints CRLF. But if it sees
	CRLF, it prints only one CRLF, not two CRLFs.
*/
void console_message(const char *s) {
    bool expect_lf = false;

    while (*s) {
        if (*s == '\r') {
            uart2_putchar(NULL, '\r');
            uart2_putchar(NULL, '\n');
            expect_lf = true;
        } else if (*s == '\n') {
            if (!expect_lf) {
                uart2_putchar(NULL, '\r');
                uart2_putchar(NULL, '\n');
            }
            expect_lf = false;
        } else {
            uart2_putchar(NULL, *s);
            expect_lf = false;
        }
        s++;
    }
}

/*
	console_print takes a pointer to a string and zero or more literal
	arguments. For each literal argument there must be a conversion
	specification in the string. The conversion specifier begins with a percent
	symbol. Simple examples are: %d for signed integer, %u for unsigned integer,
	%x for hexadecimal, %s for a string, %c for a character, %f for a floating
	point number or or a double-length floading point number. In the case of the
	"f" format specifier, we can have %10.3f for width ten characters, padded
	with spaces on the left, and three digits after the decimal point. The
	routine uses the machinery provided by stdarg.h to go through the format
	specficiations and literal arguments. This machinery is: va_start, va_list,
	vsnprintf, and va_end.
*/
void console_print(const char* fmt, ...) {
    char buff[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buff, sizeof(buff), fmt, args);
    va_end(args);
    console_message(buff);
}

/*
	console_dump_hex prints the charactesr of a null-terminated string as pairs
	of hexadecimal digits so we can see what the values of the non-printable
	characters.
*/
void console_dump_hex(const char* s) {
    const unsigned char* p = (const unsigned char*) s;
    while (*p != '\0') {
        console_print("%02X ", *p);
        p++;
    }
    console_message("\r\n");
}

/*
	console_dump_ascii prints the charactesr of a null-terminated string one
	after the other, but transforms non-printable characters into periods.
*/
void console_dump_ascii(const char* s) {
    const unsigned char* p = (const unsigned char*) s;
    while (*p != '\0') {
        if (is_printable(*p)) {
            uart2_putchar(NULL, (char) *p);
        } else {
            uart2_putchar(NULL, '.');
        }
        p++;
    }
    console_message("\r\n");
}

/*
	SYS_CONSOLE_Write is a routine used by the Microchip system processes. It
	writes a raw byte array write to the console, with no regard to the contents
	of the string, including no recognition of a null character as a terminator.
	We pass it an index to select a console, which in our case we ignore. We
	pass it a pointer to the byte array and we specify the size of the array.
*/
void SYS_CONSOLE_Write(int index, const void* buff, size_t size) {
    console_write(buff, size);
}

/*
	SYS_CONSOLE_Message is a routine used by Microchip system processes. It
	prints a literal string to the console. Its first argument is an index that
	selects the console to be printed to, but we have only one console, so we
	ignore the index. The next argument is a pointer to a null-terminated
	string. We pas that pointer to our own console_message routine.
*/
void SYS_CONSOLE_Message(int index, const char* msg) {
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
void SYS_CONSOLE_Print(int index, const char* fmt, ...) {
	(void) index;
    char buff[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buff, sizeof(buff), fmt, args);
    va_end(args);
    console_message(buff);
}

/*
	SYS_CONSOLE_ReadCountGet is a routine used by the Microchip system processes.
	It returns the number of bytes available for reading in the console receive
	buffer. We pass it a console index, but here we ignore that index.
*/
int SYS_CONSOLE_ReadCountGet(int index) {
    (void) index;
    return uart2_readcount(NULL);
}

/*
	SYS_CONSOLE_Read is a routine used by the Microchip system processes. It reads
	up to size bytes from the console receive buffer. We pass the routine a
	console index, a pointer to a buffer in memory, and the number of bytes to
	be read. We ignore the console index. If there are fewer than size bytes
	available, the routine returns all available bytes, but makes no effort to
	indicate how many bytes were read. It does not add a null terminator. The
	way Microchip uses this routine is to first use SYS_CONSOLE_ReadCountGet, so
	that it already knows how many bytes are available, and then it reads those
	available bytes. The routine transfers the bytes from the console receive
	buffer into the buffer.
*/
void SYS_CONSOLE_Read(int index, void* buff, size_t size) {
    (void) index;
    uint8_t* p = (uint8_t*) buff;
    for (size_t i = 0; i < size; i++) {
        if (uart2_readcount(NULL) == 0) {
            return;
        }
        p[i] = uart2_getchar(NULL);
    }
}

/*
	SYS_CONSOLE_Tasks is a routine used by the Microchip system processes. It is
	intended to perform a polling task. We are not going to do any polling for
	the Microchip processes, so our implementation of the routine does nothing.
*/
void SYS_CONSOLE_Tasks(int index) {
}

/*
	SYS_CONSOLE_Tasks is a routine used by the Microchip system processes. It is
	intended to perform polling tasks. We are not going to do any polling for
	the Microchip processes, so our implementation of the routine does nothing.
*/
void SYS_CONSOLE_Task(int index) {
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
	cli_debug is a Command-Line Interpreter (CLI) command that sets, clears, or
	reports the state of the global debug flag.
*/
void cli_debug(cli_chan_type *ch, char *args) {
	bool print_info = false;
	bool print_help = false;
	bool verb_received = false;
	bool set_flag = true;

	char* tok = strtok(args, " \t");
	while (tok != NULL) {
 		if (strcmp(tok, "--info") == 0) {
 			print_info = true;
 		} else if (strcmp(tok, "--help") == 0) {
 			print_help = true;
 		} else if (strcmp(tok, "set") == 0 || strcmp(tok, "clear") == 0) {
 			if (verb_received) {
				cli_print(ch,"ERROR: Only one command verb permitted in %s.\n",
					__func__);
				return;			
 			}
 			if (strcmp(tok, "clear") == 0) {
 				set_flag = false;
 			}
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
"  debug [set|clear] [--info] [--help]\n"
"\n"
"Summary:\n"
"  Sets or clears the debug flag that is used to enable console print commands\n"
"  for debugging. The initial state of this flag on reset is set by a constant in\n"
"  the source code. We can change the flag during run-time with this routine. If\n"
"  we pass no options to the routine, it returns the state of the flag: true for\n"
"  set and false for cleared.\n"
"\n"
"Verbs:\n"
"  set           Set the debug flag, debug prints enabled.\n"
"  clear         Clear the debug flag, debug prints disabled.\n"
"\n"
"Options:\n"
"  --info        Print a one-line summary of this command.\n"
"  --help        Print this help text.\n"
		);
		return;
	}

	if (!verb_received) {
		if (debug) {
			cli_message(ch, "TRUE\n");
		} else {
			cli_message(ch, "FALSE\n");
		}
	} else {
		debug = set_flag;
	}
	
    return;
}
