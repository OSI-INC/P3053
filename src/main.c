/*
	The Embedded Ethernet Module (EEM) Main Program.

	This main program is part of the P3053A repository, which began as a copy of
	a Microchip Harmony3 echo server example application. We consolidated and
	dramatically simplified the build system. There is only one Makefile left.
	We transformed the application code to create our own LWDAQ and Telnet
	servers with UART console. The lower level source files remain, for the most
	part, untouched with their Microchip copyright statements intact. We release
	this program under the GNU General Public License, which is more strict than
	the Microsoft license.

	Copyright (C) 2018 Microchip Technology Inc. and its subsidiaries. 
	Copyright (C) 2025, Kevan Hashemi, Open Source Instruments Inc.

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

// Include files.
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>				  
#include <stdbool.h>
#include <stdlib.h>
#include <stdarg.h>
#include "configuration.h"
#include "definitions.h"
#include "utils.h"
#include "pic.h"
#include "console.h"
#include "server.h"
#include "lwdaq.h"

/*
	Declare the constants and structures that configure our Telnet server. The
	Telnet server we assume will be available to any implementation of the
	A3053, provided that the VERBOSE_CONSOLE macro is defined, which is our way
	of enabling debugging features.
*/
#define TELNET_PORT 23
tcpip_server_type telnet_server = {
	INVALID_SOCKET,
	S_WAIT_STACK,
	TELNET_PORT,
	"TELNET"
};
#ifdef VERBOSE_CONSOLE
static const bool telnet_enable = true;
#else
static const bool telnet_enable = false;
#endif

/*
	telnet_tasks services a Telnet connection.
*/
int telnet_tasks(tcpip_server_type* s) {
	static uint8_t rx_buffer[TCP_RX_BUFF_SIZE];
    static char msg_buffer[TCP_TX_BUFF_SIZE];
    int len = 0;
	 
	if ((*s).state == S_LISTENING) {
		if (debug) console_print("Initialized %s connection in %s.\r\n",
			(*s).protocol, __func__);
		server_info(&msg_buffer[0], sizeof(msg_buffer));
		tcp_write_all((*s).socket, (const uint8_t*) &msg_buffer[0], 
			strlen(msg_buffer));
		return 0;
	};

	len = TCPIP_TCP_GetIsReady((*s).socket);
	if (len>0) {
		TCPIP_TCP_ArrayGet((*s).socket, &rx_buffer[0], len);	
		tcp_write_all((*s).socket, &rx_buffer[0], len);
		if (debug) console_print("Echoed %u bytes in %s.\r\n", len, __func__);
		return len;
	}

	return 0;
}

/*
	The main program, from which all other programs are called. In our case, we 
	initialize the PIC processes, then enter an infinite loop from which there
	should be no escape. So far as we can tell, the main program should return
	an integer, so we return EXIT_FAILURE if ever the program escapes our loop.
*/
int main (void) {

	/*
		Initialize the microcontroller and its peripherals, which include clock,
		memory, core timers, system timer, UART console, Ethernet physical
		interface, and the TCP/IP stack. Configure the general-purpose
		input-output pins to communicate with indicator lamps, test pins, and
		the eight-bit parallel bus.
	*/
	pic_initialize();
	
	/*
		Set up the embedded ethernet module for this particular appliction. Here
		we turn on both the green LED (D2) and the red LED (D5) on an A3053A.
		The white and blue LED pins are used for our UART2 console. We will use
		D2 as a power indicator, turning it on for 10% of the time so quickly
		that it looks constant and dim so long as the loop is executing quickly.
		We will use D5 as a heartbeat, flashing it every one or two seconds.
		This flash will be disrupted if the loop tasks start to take hundreds of
		milliseconds to complete.
	*/
   	pic_d2_on();
   	pic_d5_on();
 
   	/*
   		This loop should never terminate. We perform maintenance tasks one after
   		another. A loop counter allows us to blink lights to show heartbeat and 
   		power.
   	*/
	int i = 0;
	while (true) {
		tcpip_tick();
		console_server();
		tcpip_server(&lwdaq_server, lwdaq_tasks);
		if (telnet_enable) {
			tcpip_server(&telnet_server, telnet_tasks);
		}

		i = i+1;
		if (i % 100 == 0) pic_d2_off();
		if (i % 1000 == 0) pic_d2_on();
		if (i % 10000 == 0) pic_d5_off();
		if (i == 500000) {
			pic_d5_on();
			i = 0;
		}
	}
	
	return (EXIT_FAILURE);
}


