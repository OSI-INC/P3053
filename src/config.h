/*
	config.h -- Embedded Ethernet Module Configuration File Interface

	Allows us to configure the EEM software so that it will provide a particular
	set of services for a particular target. When we introduce variations of the
	EEM hardware, we can select which variant we are compiling for with flags in
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

/*
	When we want to provide a global flag that we can subsequently change in
	software, we give it a default value, and initialize a global variable with
	this default value.
*/
#define EEM_DEBUG_DEFAULT false
extern bool debug;

/*
	Some features we want to be able to turn off for certaing. We have compiler
	macros to make sure they are turned off. If we leave them turned on, we
	might still be able to turn them off in software or with the EEM
	configuration.
*/
#define EEM_TELNET_ENABLE true

/*
	Default server configurations.
*/
#define DEFAULT_TELNET_PORT 23

/*
	The length of the TCP socket queue. This is the number of sockets that
	can be opened by a server. Only the socket at the front of the queue
	gets serviced. The others wait.
*/
#define EEM_TCB_QUEUE_SIZE 4

/*
	The power-up wait delays checking the configuration switch until the
	motherboard is ready. Note that the power-up wait will not be effective when
	we load the EEM into a powered mPCIe socket because the contacts will be
	unreliable as the EEM is powering up. When we plug the EEM into a life mPCIe
	socket, we might see an unwanted reset to factory default configuration.
*/
#define EEM_POWERUP_WAIT_MS 200

/*
	The module is the EEM circuit board. The A3053A is the first EEM circuit
	board, with an inconvenient allocation of PIC32MZ pins to the mPCIe bus. The
	A3053B is the second EEM, which has a more convenient allocation. We define
	a name string for the module and a compiler macro that contains the module
	name. We use the macro to perform conditional compilation that configures
	the EEM for each particular module. 
	
	Embedded Ethernet Module             A3053A
	Embedded Ethernet Module             A3053B
	
	We define a macro below and use it to define a name string for each of the
	module versions listed in the table above.
*/
#define EEM_MODULE_A3053A

#ifdef EEM_MODULE_A3053A
#define EEM_MODULE_NAME "A3053A"
#endif

#ifdef EEM_MODULE_A3053B
#define EEM_MODULE_NAME "A3053B"
#endif

#ifndef EEM_MODULE_NAME
#define EEM_MODULE_NAME "UNKNOWN"
#endif

/*
	The motherboard is the board that the EEM plugs into. In our case, this
	might be a LWDAQ Driver with Ethernet Interface, a an Animal Location
	Tracker, a Telemetry Control Box, or an Analog Signal Generator, to name a
	few. We define a name string for the motherboard and a compiler macro that
	contains the motherboard name. We use the macro to perform conditional
	compilation that configures the EEM for each particular motherboard. Here
	are the assembly numbers the EEM code recognises.
	
	LWDAQ Driver with Ethernet Interface  A2071
	TCPIP-VME Interface                   A2087
	Animal Location Tracker               A3038
	Telemetry Control Box                 A3042
	Function Generator                    A3050
	Analog Signal Generator               A3052
	
	We use the motherboard macro to define a motherboard name string for each of
	the motherboard versions listed in the table above.
*/
#define EEM_MOTHERBOARD_A2071

#ifdef EEM_MOTHERBOARD_A2071
#define EEM_MOTHERBOARD_NAME "A2071"
#endif

#ifdef EEM_MOTHERBOARD_A2087
#define EEM_MOTHERBOARD_NAME "A2087"
#endif

#ifdef EEM_MOTHERBOARD_A3038
#define EEM_MOTHERBOARD_NAME "A3038"
#endif

#ifdef EEM_MOTHERBOARD_A3042
#define EEM_MOTHERBOARD_NAME "A3042"
#endif

#ifdef EEM_MOTHERBOARD_A3050
#define EEM_MOTHERBOARD_NAME "A3050"
#endif

#ifdef EEM_MOTHERBOARD_A3052
#define EEM_MOTHERBOARD_NAME "A3052"
#endif

#ifndef EEM_MOTHERBOARD_NAME
#define EEM_MOTHERBOARD_NAME "UNKNOWN"
#endif

/*
	When our GitHub repository version is X.Y, we let the software version we report
	for our EEM be 32+X. We do this so that we can tell the difference between LWDAQ
	Relays equipped with the RCM6700 from those equipped with the EEM. The RCM6700
	relays have software version numbers less than 32.
*/
#define EEM_SOFTWARE_VERSION 33

/*
	Default configuration of the CLI.
*/
#define CLI_PROMPT_DEFAULT "EEM % "

#endif 
