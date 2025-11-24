/*
	stubs.h declares miscellaneous routines we have written to replace Harmony3 routines.
*/

#ifndef STUBS_H
#define STUBS_H

/*
	A random number generator for Harmony to use, replaces the routine of the
	same name that was provided by their cryptography system, which we removed.
*/
uint32_t SYS_RANDOM_PseudoGet(void);

#endif
