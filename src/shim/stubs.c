#include <stdlib.h>
#include <stdint.h>

/*
	SYS_RANDOM_PseudoGet replaces the cryptographic random number generator that
	Harmony's DHCP process used to introduce jitter into its delays. We removed
	all the cryptography and secure socket layer code from our project, but this
	was one routine that had to be supported.
*/
uint32_t SYS_RANDOM_PseudoGet(void) {
    return (uint32_t)rand();
}

