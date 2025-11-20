/*
	console.c is a library of routines that implement a console and
	command interface using our UART2 interface. It defines a family
	of console_* procedures for writing to and reading from the UART2.
	It provides implementation for the family of SYS_CONSOLE routines
	that Harmony system modules call for reporting and debugging.
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
#include "comms.h"
#include "console.h"

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
    while (UART2_Write((uint8_t*)&c, 1) == 0); 
}

/*
	console_flush waits until the UART2 has transmitted all pending characters. It first
	waits for the software ring buffer to empty, then waits until the UART's internal
	buffer is empty, then waits until the UART's transmit shift register is empty.
*/
void console_flush(void) {
    while (UART2_WriteCountGet() != 0) { ; }
    while (U2STAbits.UTXBF == 1) { ; }
    while (U2STAbits.TRMT == 0) { ; }
}

/*
	console_puthex takes a thirty-two bit integer, which is eight nibbles, and
	prints it a hexadecimal value, most significant nibble first, to the
	console.
*/
void console_puthex(uint32_t value) {
    const char hex[] = "0123456789ABCDEF";
    for (int shift = 28; shift >= 0; shift -= 4)
        console_putchar(hex[(value >> shift) & 0xF]);
}

/*
	console_putint is not an ASCII-reporting routine. It is designed to allow us
	to view thirty-two bit integers on an oscilloscope attached to our console
	TX signel. It takes an integer value and transmits it, least significant
	byte first, over the UART. Because the UART sends the least significant bit
	first, we will see the bits on our oscilloscope trace of UART2 TX line as
	bit 0 to 32, with a stop bit (1) and a start bit (0) between each of the
	bytes. We can use it to broadcast raw register values, as in
	console_putint(RPF3R). We used this routine to look at LATF, PIRTF, ODCF,
	and RPF3R when we were investigating our start-up GPIP problems. By looking
	at the bits on an oscilloscope, we do not need a functioning UART-to-USB
	bridge, and we can view register bits toggling more easily.		
*/
void console_putint(uint32_t value) {
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
	a null character, which it does not write.
*/
void console_message(const char *s) {
    while (*s) console_putchar(*s++);
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
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    console_message(buf);
}

/*
	console_readcount returns the number of bytes available for reading in the
	console receive buffer.
*/
int console_readcount(void) {
    return UART2_ReadCountGet();
}

/*
	console_getchar reads one character from the console. It uses the UART2_Read
	routine, defined in plib_uart2.c. It returns either a character or the value
	minus one, which indicates no character was read. This routine can be
	polled, so that when it returns characters, we keep reading until we get a
	minus one.
*/
int console_getchar(void) {
    uint8_t c;
    if (UART2_Read(&c, 1) == 1) return c;
    return -1; 
}

int console_readln(char* buf, int maxlen)
{
    int idx = 0;

	while (1) {
        char c = console_getchar();
        if (c == '\r' || c == '\n') {
            console_message("\r\n");
            buf[idx] = '\0';
            return idx;
        } else if (c == '\b' || c == 0x7F) {
            if (idx > 0) {
                idx--;
                console_message("\b \b");
            }
        } else if (idx < maxlen - 1) {
        	if (!is_printable(c)) continue;
			console_putchar(c);
			buf[idx++] = c;
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
    console_write(buff,size);
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
	using the stdarg.h machinery.
*/
void SYS_CONSOLE_Print(int index, const char* fmt, ...) {
	(void) index;
	char buffer[256];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, ap);
	va_end(ap);
	console_message(buffer);
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


*/
#define CMD_MAX_LEN 64
static char cmd_buffer[CMD_MAX_LEN];
static unsigned cmd_len = 0;

void console_initialize(void)
{
    cmd_len = 0;
	console_print("\r\n\r\n");
	console_print("===========================================================\r\n");
	console_print("=========    Embedded Ethernet Module (A3053)     =========\r\n");
	console_print("===========================================================\r\n");
	console_print("Command interface running, 'h' for help, 'r' for reset.\r\n");
}

void console_server(void)
{
    char c;
    char buff[20];

	while (console_readcount() > 0) {
		c = console_getchar();
		if (c == '\r' || c == '\n') {
			console_message("\r\n");
			if (cmd_len == 1) {
				char cmd = cmd_buffer[0];
				switch (cmd) {
					case 'h':
						console_message("Commands:\r\n");
						console_message("  a - new ip addr\r\n");
						console_message("  h - print help\r\n");
						console_message("  n - net info\r\n");
						console_message("  p - ping gateway\r\n");
						console_message("  r - software reset\r\n");
						break;
						
					case 'a':
						console_message("New IP Address: ");
						console_readln(buff,sizeof(buff));
						console_print("%s\r\n",buff);
						break;
						
					case 'n':
						net_info();
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
						console_print("ERROR: Unknown command '%c'.\r\n", cmd);
						break;
				}
			} else if (cmd_len > 1) {
				console_message("ERROR: Only single-letter commands supported.\r\n");
			}
		cmd_len = 0;
		console_message("EEM$ ");
		continue;
		}
		if (c == '\b' || c == 0x7F) {
			if (cmd_len > 0) {
			cmd_len--;
			console_message("\b \b");
			}
			continue;
		}
		if (!is_printable(c)) continue;
		console_print("%c", c);
		if (cmd_len < CMD_MAX_LEN) {
			cmd_buffer[cmd_len++] = c;
		} else {
			cmd_len = 0;
			console_message("\r\nError: command too long\r\n> ");
		}
	}
}
