/*
	utils.h declares variables, types, and functions used by our generic utility routines.
*/

#ifndef UTILS_H
#define UTILS_H

/*
	flip_bytes_u32 reverses the order of four bytes in a thirty-two bit unsigned
	integer so as to convert little-endian to big-endian byte order, and
	visa-versa.
*/
static inline uint32_t flip_bytes_u32(uint32_t x)
{
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
static inline uint32_t reverse_load_u32(const uint8_t *p)
{
	return ((uint32_t)p[0] << 24) 
		| ((uint32_t)p[1] << 16) 
		| ((uint32_t)p[2] <<  8) 
		| (uint32_t)p[3];
}

/*
	String manipulation procedures.
*/
const char* string_trim(const char *s);

#endif
