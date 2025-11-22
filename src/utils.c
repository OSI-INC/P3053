/*
	utils.c is a library of generic, platform-independent utility routines.
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



