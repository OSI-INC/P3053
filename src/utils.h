/*
	utils.h -- Interface of the General-Purpose Utility library. These routines
	do not use any Microchip or EEM-specific functions, only the standard
	C-Languate run-time library.

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

#ifndef UTILS_H
#define UTILS_H

/*
	flip_bytes_u32 reverses the order of four bytes in a thirty-two bit unsigned
	integer so as to convert little-endian to big-endian byte order, and
	visa-versa.
*/
static inline uint32_t flip_bytes_u32(uint32_t x) {
    return ((x & 0x000000FFu) << 24) |
		((x & 0x0000FF00u) <<  8) |
		((x & 0x00FF0000u) >>  8) |
		((x & 0xFF000000u) >> 24);
}

/*
	reverse_load_u32 takes a pointer to a byte buffer and reads the byte
	pointed to as the most significant byte of a four-byte unsigned integer, and
	the next three bytes as the second, third, and fourth bytes of a big-endian
	unsigned integer and returns then as a four-byte little-endian unsigned
	integer.
*/
static inline uint32_t reverse_load_u32(const uint8_t *p) {
	return ((uint32_t)p[0] << 24) 
		| ((uint32_t)p[1] << 16) 
		| ((uint32_t)p[2] <<  8) 
		| (uint32_t)p[3];
}

/*
	An in-line routine to test a character to see if it is printable.
*/
static inline bool is_printable(char c) {
    return (c >= 32 && c <= 126);
}

/*
	String manipulation and checking procedures.
*/
const char *string_trim(const char *s);
bool parse_uint8(const char *tok, uint8_t *out_ptr);
bool parse_uint32(const char *tok, uint32_t *out_ptr);

#endif
