/*
	config.h -- Embedded Ethernet Module Configuration File Interface

	Allows us to configure the EEM software so that it will provide a particular
	set of services for a particular target. We can enable or disable voluntary
	debug reporting to the console. We can enable or disable the Telnet
	command-line interface server. When we introduce variations of the EEM
	hardware, we can select which variant we are compiling for with flags in
	this header. We make a distinction between flags that change the way the
	code is compiled and flags that change the way the code operates. In our
	debug reporting code, for example, we use a "debug" flag that the process
	checks at run-time to determine whether or not to make a debug print. This
	class of configuration we call an "operation flag", and all such flags are
	lower-case. In our hardware variant management, we use compiler macros to
	create "compiler directive". All these compiler macros will be upper-case
	and begin with "EEM".
	
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

#ifndef CONFIG_H
#define CONFIG_H

#define EEM_DEBUG_DEFAULT false
extern bool debug;

#define EEM_TELNET_ENABLE_DEFAULT true
extern bool telnet_enable;

#define EEM_PLATFORM "A3053A"
#define EEM_HOST "A3042"
#define EEM_RELAY_VERSION 32

#endif 
