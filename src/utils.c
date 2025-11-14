/*
	utils.c is a library of utility routines that communicate with the PIC32MZ
	registers, read and write throught he MPCIE parallel port, and perform
	various mundane tasks with strings and buffers.
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

/*
	string_trim takes a pointer to a null-terminated string and returns a pointer
	to another null-terminated string that is the same as the original but with all
	whitespace removed from the beginning and the end.
*/
const char* string_trim(const char *s) {
    static char buf[256];
    const char *start = s;
    const char *end;
    size_t len;

    if (s == NULL) return "";
    while (*start && isspace((unsigned char)*start)) start++;
    end = start + strlen(start);
    while (end > start && isspace((unsigned char)*(end - 1))) end--;
    len = end - start;
    if (len >= sizeof(buf)) len = sizeof(buf) - 1;
    memcpy(buf, start, len);
    buf[len] = '\0';
    return buf;
}

/*
	eem_reset resets the Embedded Etherent Module. It does so by unlocking the
	PIC32MZ configuration registers and writing to the reset configuration bit.
	We make three writes to the thirty-two bit SYSKEY register with the correct
	combination to unlock the configuration bits. Then we set the RSWRSTSET
	register equal to a value defined in the device pack: "_RSWRST_SWRST_MASK".
	We read the reset register after that, which completes the initiation of
	reset. The routine does not return, it puts itself in an infinite loop,
	trusting that the reset will take place and reboot the system.
*/
void eem_reset(void) {
    SYSKEY = 0x00000000;
    SYSKEY = 0xAA996655;
    SYSKEY = 0x556699AA;
    RSWRSTSET = _RSWRST_SWRST_MASK;
    (void) RSWRST;
    while(1);
}
