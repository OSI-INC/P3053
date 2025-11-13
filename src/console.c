#include <stdio.h>
#include <stdint.h>
#include <stddef.h>                  
#include <stdbool.h>
#include <stdlib.h>
#include <stdarg.h>
#include "configuration.h"
#include "definitions.h"
#include "console.h"

/*
	console_putc writes one character to the console. It calls UART2_Write,
	which is defined in plib_uart2.c. We are using UART2 for our console. The
	UART2_Write routine returns zero if the character write failes, so we wait
	until it returns non-zero, and at that point we know the character write has
	succeeded. Thus console_putc blocks until completed.
*/
void console_putc (char c)
{
    while (UART2_Write((uint8_t*)&c, 1) == 0); 
}

/*
	console_puts writes an entire null-terminated string of characters to the
	console.
*/
void console_puts (const char *s)
{
    while (*s) console_putc(*s++);
}

/*
	console_printf takes a string that may or may not contain format codes, and
	then gathers variables to match the format codes, and so creates a string
	that it writes to the console. The routine uses the machinery provided by
	stdarg.h: va_start, va_list, and va_end.
*/
void console_printf(const char* fmt, ...)
{
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    console_puts(buf);
}

/*
	console_puthex takes a thirty-two bit integer, which is eight nibbles, and
	prints it a hexadecimal value, most significant nibble first, to the
	console.
*/
void console_puthex(uint32_t value)
{
    const char hex[] = "0123456789ABCDEF";
    for (int shift = 28; shift >= 0; shift -= 4)
        console_putc(hex[(value >> shift) & 0xF]);
}

/*
	console_putint is not an ASCII-reporting routine. It is designed to allow
	us to view thirty-two bit integers on an oscilloscope attached to our
	console TX signel. It takes an integer value and transmits it, least
	significant byte first, over the UART. Because the UART sends the least
	significant bit first, we will see the bits on our oscilloscope trace of
	UART2's TX line as bit 0 to 32, with a stop bit (1) and a start bit (0)
	between each of the bytes. We can use it to broadcast raw register values,
	as in:

	console_send_int(RPF3R);
	console_send_int(LATF);
	console_send_int(PORTF);
	console_send_int(ODCF);
	console_send_int(RPF3R);

	By this means, we do not need a UART-to-USB bridge to see raw register
	contents, and we can view register bits toggling more easily.		
*/
void console_putint(uint32_t value)
{
	int i;
	for (i = 0; i <= 3; i++) console_putc(((uint8_t*)&value)[i]);
}

/*
	console_getchar reads one character from the console. It uses the UART2_Read
	routine, defined in plib_uart2.c. It returns either a character or the value
	minus one, which indicates no character was read. This routine can be
	polled, so that when it returns characters, we keep reading until we get a
	minus one.
*/
int console_getchar(void)
{
    uint8_t c;
    if (UART2_Read(&c, 1) == 1) return c;
    return -1; 
}


void SYS_CONSOLE_Print(int index, const char* fmt, ...)
{
    // no-op
}

void SYS_CONSOLE_Message(int index, const char* msg)
{
    // no-op
}

void SYS_CONSOLE_Write(int index, const void* buff, size_t size)
{
    // no-op
}

void SYS_CONSOLE_Read(int index, void* buff, size_t size)
{
    // no-op
}

void SYS_CONSOLE_Tasks(int index)
{
    // no-op
}

void SYS_CONSOLE_Task(int index)
{
    // no-op
}

int SYS_CONSOLE_ReadCountGet(int index)
{
    return 0;
}
