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
#include "console.h"

/*
	An in-line routine to test a character to see if it is printable.
*/
static inline bool is_printable(char c) {
    return (c >= 32 && c <= 126);
}

/*
	console_putchar writes one character to the UART2 transmit buffer. It calls
	UART2_Write from plib_uart2.c, which returns zero if the transmit buffer is
	full. We wait until the routine returns non-zeo, at which point we know our
	character was written to the transmit buffer. Thus console_putchar blocks
	until we are able to write to the buffer, but note that it does not block
	waiting for the character to be transmitted by the UART. If we want to wait
	for all characters to be transmitted, we use our flush procedure.
*/
void console_putchar(char c) {
	uart2_putchar(NULL, c);
}

/*
	console_flush waits until the UART2 has transmitted all pending characters.
	It first waits for the software ring buffer to empty, then waits until the
	UART's internal buffer is empty, then waits until the UART's transmit shift
	register is empty. We use this routine when we want to pause before doing
	some other task until all pending console printing is done, such as before
	we reset the PIC.
*/
void console_flush(void) {
	uart2_flush(NULL);
}

/*
	console_put_int_hex takes a thirty-two bit integer, which is eight nibbles,
	and prints it as eight hexadecimal digits, most significant nibble first, to
	the console.
*/
void console_put_int_hex(uint32_t value) {
    const char hex[] = "0123456789ABCDEF";
    for (int shift = 28; shift >= 0; shift -= 4)
        console_putchar(hex[(value >> shift) & 0xF]);
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
	for (i = 0; i <= 3; i++) console_putchar(((uint8_t*)&value)[i]);
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
        console_putchar(p[i]);
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
            console_putchar((char) *p);
        } else {
            console_putchar('.');
        }
        p++;
    }
    console_message("\r\n");
}

/*
	console_readcount returns the number of bytes available for reading in the
	console receive buffer.
*/
int console_readcount(void) {
    return uart2_readcount(NULL);
}

/*
	console_getchar reads one character from the console. It uses the UART2_Read
	routine, defined in plib_uart2.c. It returns either a character or minus
	one, which indicates no character was read. This routine can be polled, so
	that when it returns characters, we keep reading until we get a minus one.
*/
int console_getchar(void) {
    return uart2_getchar(NULL);
}

/*
	console_readln reads from the console until it comes to a newline character.
	Until such time, it blocks the processor, so only interrupts will be
	services. When it receives the newline, it echoes characters and adds them
	to the buffer it was passed as an argument. It makes sure it does not write
	more than maxlen-1 characters to the buffer, and it terminates the string
	with a null character.
*/
int console_readln(char* buff, int maxlen) {
    int idx = 0;

	while (1) {
        char c = console_getchar();
        if (c == '\r' || c == '\n') {
            console_message("\r\n");
            buff[idx] = '\0';
            return idx;
        } else if (c == '\b' || c == 0x7F) {
            if (idx > 0) {
                idx--;
                console_message("\b \b");
            }
        } else if (idx < maxlen - 1) {
        	if (!is_printable(c)) continue;
			console_putchar(c);
			buff[idx] = c;
			idx++;
        }
    }
}

/*
	SYS_CONSOLE_Write is a routine used by the Harmony system processes. It
	writes a raw byte array write to the console, with no regard to the contents
	of the string, including no recognition of a null character as a terminator.
	We pass it an index to select a console, which in our case we ignore. We
	pass it a pointer to the byte array and we specify the size of the array.
*/
void SYS_CONSOLE_Write(int index, const void* buff, size_t size) {
    console_write(buff, size);
}

/*
	SYS_CONSOLE_Message is a routine used by Harmony system processes. It
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
	SYS_CONSOLE_Print is a routine used by the Harmony system processes. It
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
	SYS_CONSOLE_ReadCountGet is a routine used by the Harmony system processes.
	It returns the number of bytes available for reading in the console receive
	buffer. We pass it a console index, but here we ignore that index.
*/
int SYS_CONSOLE_ReadCountGet(int index) {
    (void) index;
    return console_readcount();
}

/*
	SYS_CONSOLE_Read is a routine used by the Harmony system processes. It reads
	up to size bytes from the console receive buffer. We pass the routine a
	console index, a pointer to a buffer in memory, and the number of bytes to
	be read. We ignore the console index. If there are fewer than size bytes
	available, the routine returns all available bytes, but makes no effort to
	indicate how many bytes were read. It does not add a null terminator. The
	way Harmony uses this routine is to first use SYS_CONSOLE_ReadCountGet, so
	that it already knows how many bytes are available, and then it reads those
	available bytes. The routine transfers the bytes from the console receive
	buffer into the buffer.
*/
void SYS_CONSOLE_Read(int index, void* buff, size_t size) {
    (void) index;
    uint8_t* p = (uint8_t*) buff;
    for (size_t i = 0; i < size; i++) {
        if (console_readcount() == 0) {
            return;
        }
        p[i] = console_getchar();
    }
}

/*
	SYS_CONSOLE_Tasks is a routine used by the Harmony system processes. It is
	intended to perform a polling task. We are not going to do any polling for
	the Harmony processes, so our implementation of the routine does nothing.
*/
void SYS_CONSOLE_Tasks(int index) {
}

/*
	SYS_CONSOLE_Tasks is a routine used by the Harmony system processes. It is
	intended to perform polling tasks. We are not going to do any polling for
	the Harmony processes, so our implementation of the routine does nothing.
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

	console_print("\r\n\r\n");
	console_print("===========================================================\r\n");
	console_print("=========    Embedded Ethernet Module (A3053)     =========\r\n");
	console_print("===========================================================\r\n");
	if (debug) {
		console_print("The VERBOSE_CONSOLE flag is set.\r\n");
	} else {
		console_print("The VERBOSE_CONSOLE flag is cleared..\r\n");	
	}
}

/*
	console_print_help prints a help message to the console for the console
	server. We call it once when the console server starts up, and any time
	the user presses "h".
*/
void console_print_help(void) {
	console_message("Commands:\r\n");
	console_message("  c - save string to flash\r\n");
	console_message("  d - read string from flash\r\n");
	console_message("  i - new ip addr\r\n");
	console_message("  m - machine configuration\r\n");
	console_message("  n - net info\r\n");
	console_message("  p - ping gateway\r\n");
	console_message("  r - software reset\r\n");
	console_message("  h - print help\r\n");
}

/*
	console_server looks out for characters sent in through the console
	interface, interpretes tham, and responds to them. It always provides an "h"
	command for help, and the help menu in the code below shows the supported
	and planned commands. The commands are all single-letter commands, but some
	of them, such as the one to change the IP address, will subsequently accept
	a string of characters. The console server is always active, regardless of
	the state of the debug flag.
*/
void console_server(void) {
	#define CONSOLE_STR_ADDR 0xBD104000
	static bool print_help = true;
	
	enum {max_cmd_len=255};
	static char cmd_buff[max_cmd_len];
	static uint32_t cmd_len = 0;
    char c;
    
	enum {max_str_len=31};
	static char ip_str[max_str_len];
	static char gw_str[max_str_len];
	static char mask_str[max_str_len];
	
    enum {max_msg_len=2048};    
    static char msg_buff[max_msg_len];
    
    int status;
	bool ignore_lf = false;
	bool process_command = false;    
    
    if (print_help) {
    	console_print_help();
    	print_help = false;
    }

	while (console_readcount() > 0) {
		c = console_getchar();
		if (c == '\r') {
			process_command = true;
			ignore_lf = true;
		} else if (c == '\n') {
			if (!ignore_lf) {
				process_command = true;
			} else {
				ignore_lf = false;
			}
		} else if (c == '\b' || c == 0x7F) {
			if (cmd_len > 0) {
				cmd_len--;
				console_message("\b \b");
			}
		} else if (is_printable(c)) {
			console_print("%c", c);
			if (cmd_len < max_cmd_len) {
				cmd_buff[cmd_len] = c;
				cmd_len++;
			} else {
				cmd_len = 0;
				console_print("\r\nERROR: Have cmd_len>%d in %s.\r\n",
					max_cmd_len, __func__);
			}
		}
		
		if (!process_command) {continue;}
		
		console_message("\r\n");
		if (cmd_len == 1) {
			char cmd = cmd_buff[0];
			switch (cmd) {
				case 'h':
					console_print_help();
					break;
					
				case 'c':
					console_message("String: ");
					console_readln(msg_buff, sizeof(msg_buff));
					status = pic_nvm_writestr(CONSOLE_STR_ADDR, msg_buff);
					if (status >= 0) {
						console_print("Wrote: %s\r\n", msg_buff);
					} else {
						console_print("ERROR: String write failed in %s.\r\n",
							__func__);
					}	
					break;
					
				case 'd':
					console_message("Reading string...\r\n");
					status = pic_nvm_readstr(
						msg_buff,
						CONSOLE_STR_ADDR,
						sizeof(msg_buff));
					if (status >= 0) {
						console_print("String: %s\r\n", msg_buff);
					} else {
						console_print("ERROR: String read failed in %s.\r\n",
							__func__);
					}
					break;
					
				case 'i':
					console_message("New IP Address: ");
					console_readln(ip_str, sizeof(ip_str));
					console_message("New IP Mask: ");
					console_readln(mask_str, sizeof(mask_str));
					console_message("New Gateway: ");
					console_readln(gw_str, sizeof(gw_str));
					server_set_ip(ip_str, gw_str, mask_str);
					break;
					
				case 'm':
					pic_info(msg_buff);
					console_print("%s\r\n", msg_buff);
					break;
				
				case 'n':
					server_info(msg_buff);
					console_print("%s\r\n", msg_buff);
					break;
				
				case 'p':
					ping_gateway();
					break;
				
				case 'r':
					console_message("Resetting... \r\n");
					console_flush();
					pic_reset();
					break;
				
				default:
					console_print(
						"ERROR: Unknown command '%c', type 'h' for help in %s.\r\n",
						cmd, __func__);
					break;
			}
		} else if (cmd_len > 1) {
			console_print("ERROR: Single-letter commands only in %s.\r\n",
				__func__);
		}
		cmd_len = 0;
		console_message("EEM$ ");
	}
}
