/*
	stubs.c -- Implementation of our Stubs Library. 

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

#include <stdlib.h>
#include <stdint.h>

/*
	SYS_RANDOM_PseudoGet replaces the cryptographic random number generator that
	the Microchip DHCP process uses to introduce jitter into its delays. We
	removed all the cryptography and secure socket layer code from our project,
	but this was one routine that had to be supported.
*/
uint32_t SYS_RANDOM_PseudoGet(void) {
    return (uint32_t)rand();
}

