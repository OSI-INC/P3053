/*
	stubs.h -- Interface to our Stubs Library. Contains routines that replace
	routines in provided by Microchip library files we have removed from our
	repository.

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

#ifndef STUBS_H
#define STUBS_H

/*
	A random number generator for the Microchip library to use, replaces the
	routine of the same name that was provided by their cryptography system,
	which we removed.
*/
uint32_t SYS_RANDOM_PseudoGet(void);

#endif
