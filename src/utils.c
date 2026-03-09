/*
	utils.c -- Implementation of the General-Purpose Utility library.

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
	The standard C headers we trust the compiler will know how to find.
*/
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>                  
#include <stdbool.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#include "utils.h"

/*
	string_trim takes a pointer to a null-terminated string and returns a
	pointer to another null-terminated string that is the same as the original
	but with all whitespace removed from the beginning and the end. The routine
	is not re-entrant: the trimmed string must be copied or have served its
	purpose before the next call to string_trim, because it is a static string
	allocated for the routine in memory. 
*/
const char *string_trim(const char *s) {
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
	parse_uint8 attempts to parse a string into a valid unsigned eight-bit
	integer pointed to by out_ptr. If it fails, it does not write to the output
	byte, but instead returns false.
*/
bool parse_uint8(const char *tok, uint8_t *out_ptr) {
    char *end;
	if (!tok || !out_ptr) return false;
    if (*tok == '-') return false;
    unsigned long v = strtoul(tok, &end, 0);
    if (*tok == '\0' || *end != '\0' || v > 0xFF) return false;
    *out_ptr = (uint8_t) v;
    return true;
}

/*
	parse_uint32 attempts to parse a string into a valid unsigned thirty-two-bit
	integer pointed to by out_ptr. If it fails, it does not write to the output
	byte, but instead returns false.
*/
bool parse_uint32(const char *tok, uint32_t *out_ptr) {
    char *end;
	if (!tok || !out_ptr) return false;
    if (*tok == '-') return false;
    unsigned long v = strtoul(tok, &end, 0);
    if (*tok == '\0' || *end != '\0') return false;
    *out_ptr = (uint32_t) v;
    return true;
}





