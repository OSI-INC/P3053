/*
	The Embedded Ethernet Module (EEM) Main Program.

	This program implements a LWDAQ server in our A3053 family of EEMs.

	This main program is part of the P3053 repository, which began as a copy of
	a Microchip Harmony3 echo server example application. We consolidated and
	dramatically simplified the build system. There is only one Makefile left.
	We transformed the application code to create our own LWDAQ server. We have
	consolidated all higher level functions int this one file, main.c. The lower
	level source files remain, for the most part, untouched with their Microchip
	copyright statements intact. We release this program under the GNU General
	Public License, which is more strict than the Microsoft license.

	Copyright (C) 2018 Microchip Technology Inc. and its subsidiaries. Copyright
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
#include "server.h"
#include "console.h"

// Current version number
#define VERSION_NUM 32

// Configuration constants.
#define ETH_MTU 1514
#define BUFF_SIZE (ETH_MTU-40)
#define RAM_BUFF_SIZE (6*BUFF_SIZE)
#define TCP_BUFF_SIZE (6*BUFF_SIZE)
#define CONFIG_LENGTH 1024 // bytes for config file buffer
#define SEPCHARS " :\n,;=" // separator characters in config file

// Global variables.
char logged_in;
int ip_port,tcp_timeout,security_level;
char password[32];
char configuration[CONFIG_LENGTH];

// Global socket variables.
#define LWDAQ_PORT 90
#define TELNET_PORT 23
#define HTTP_PORT 80

// Server state structures.
tcpip_server_type lwdaq_server = {INVALID_SOCKET,S_WAIT_STACK,LWDAQ_PORT,"LWDAQ"};
tcpip_server_type telnet_server = {INVALID_SOCKET,S_WAIT_STACK,TELNET_PORT,"TELNET"};
tcpip_server_type http_server = {INVALID_SOCKET,S_WAIT_STACK,HTTP_PORT,"HTTP" };

// LWDAQ messages
#define START_CODE 0xA5 
#define END_CODE 0x5A
#define CLOSE_CODE 0x04
#define START_OFFSET 0
#define ID_OFFSET 1
#define CLEN_OFFSET 5
#define CONTENT_OFFSET 9
#define FRAME_SIZE 10

// Message Identifiers
#define VERSION_READ	0
#define BYTE_READ 		1
#define BYTE_WRITE 		2
#define STREAM_READ		3
#define DATA_RETURN		4
#define BYTE_POLL		5
#define LOGIN			6
#define CONFIG_READ		7
#define CONFIG_WRITE	8
#define MAC_READ		9
#define STREAM_DELETE	10
#define ECHO			11
#define STREAM_WRITE	12
#define REBOOT			13

/*
	swap_u32 reverses the order of four bytes in a thirty-two bit unsigned
	integer so as to convert little-endian to big-endian byte order, and
	visa-versa.
*/
static inline uint32_t swap_u32(uint32_t x)
{
    return ((x & 0x000000FFu) << 24) |
		((x & 0x0000FF00u) <<  8) |
		((x & 0x00FF0000u) >>  8) |
		((x & 0xFF000000u) >> 24);
}

/*
	load32_be takes a pointer to a byte buffer and reads the byte pointed
	to and the three after it as the bytes of a four-byte big-endian integer
	and returns a little-endian uint32_t.
*/
static inline uint32_t load32_be(const uint8_t *p)
{
	return ((uint32_t)p[0] << 24) 
		| ((uint32_t)p[1] << 16) 
		| ((uint32_t)p[2] <<  8) 
		| (uint32_t)p[3];
}

/*
	lwdaq_header sends a data return message header through a socket.
*/
int lwdaq_header(tcpip_server_type* s, uint32_t id, uint32_t len) {
	uint8_t buff[15];
	uint32_t* lp;
	
	buff[START_OFFSET] = START_CODE;
	lp = (uint32_t*)&buff[ID_OFFSET];
	*lp = swap_u32(id);
	lp = (uint32_t*)&buff[CLEN_OFFSET];
	*lp = swap_u32(len);
	TCPIP_TCP_ArrayPut((*s).socket,buff,CONTENT_OFFSET);
	return 0;
}

/*
	lwdaq_footer sends a terminating sequence through a socket.
*/
int lwdaq_footer(tcpip_server_type* s) {
	uint8_t buff[15];
	buff[0] = END_CODE;
	TCPIP_TCP_ArrayPut((*s).socket,buff,1);
	return 0;
}

/*
	lwdaq_byte sends a data return message through a socket with a single-byte
	integer as its content.
*/
int lwdaq_byte(tcpip_server_type* s, uint8_t data) {
	uint8_t buff[15];
	lwdaq_header(s,DATA_RETURN,sizeof(data));
	buff[0] = data;
	TCPIP_TCP_ArrayPut((*s).socket,buff,sizeof(data));
	lwdaq_footer(s);
	return 0;
}

/*
	lwdaq_integer sends a data return message through a socket with a four-byte
	integer as its content.
*/
int lwdaq_integer(tcpip_server_type* s, uint32_t data) {
	uint8_t buff[15];
	uint32_t* lp;
	lwdaq_header(s,DATA_RETURN,sizeof(data));
	lp = (uint32_t*)&buff[0];
	*lp = swap_u32(data);
	TCPIP_TCP_ArrayPut((*s).socket,buff,sizeof(data));
	lwdaq_footer(s);
	return 0;
}

/*
	lwdaq_data sends a block of data through a socket.
*/
int lwdaq_data(tcpip_server_type* s, uint8_t* block, uint32_t len) {
	lwdaq_header(s,DATA_RETURN,len);
	TCPIP_TCP_ArrayPut((*s).socket,block,len);
	lwdaq_footer(s);
	return 0;
}

/*
	lwdaq_process_message handles an incoming LWDAQ message.
*/
int lwdaq_process_message (tcpip_server_type* s, 
		uint32_t id, uint32_t len, uint8_t* content) {
	uint8_t register_addr = 0;
	uint8_t value = 0;
//	int status;
	
	switch (id) {
		case BYTE_WRITE:{
			register_addr = content[3];
			value = content[4];
			//write_controller_byte(register_addr,value);
			if (debug) console_print("BYTE_WRITE to %d of %d in %s.\r\n",
				register_addr,value,__func__);
			break;
		}

		case BYTE_READ:{
			register_addr = content[3];
			value = bus_read_byte(register_addr);
			if (debug) console_print("BYTE_READ from %d of %d in %s.\r\n",
				register_addr,value,__func__);
			lwdaq_byte(s,value);
			break;
		}

		case BYTE_POLL:{
			register_addr = content[3];
			value = content[4];
			if (debug) console_print("BYTE_POLL of %d for %d in %s.\r\n",
				register_addr,value,__func__);
/*
			while (read_controller_byte(register_addr) != value) {
				status = buffered_socket_read(&socket,NULL,0);
				if (status<0) break;
			}
*/
			break;
		}

		case VERSION_READ:{
			if (debug) console_print("VERSION_READ in %s.\r\n",__func__);
			lwdaq_integer(s,VERSION_NUM);
			break;
		}

		case ECHO:{
			if (debug) console_print("ECHO of %u bytes in %s.\r\n",len,__func__);
			lwdaq_data(s,content,len);
			content[len] = 0x00;
			if (debug) console_print("%s\r\n",content);
			break;
		}

		default: {
			if (debug) console_print("Unrecognised message in %s.\r\n",__func__);
			break;
		}
	}
	return 0;
}

/*
	lwdaq_tasks services a LWDAQ connection.
*/
int lwdaq_tasks(tcpip_server_type* s) {
	uint32_t id,len;
	static uint8_t rx_buffer[TCP_BUFF_SIZE];
	static uint32_t rx_available = 0;
	uint32_t rx_ready = 0;
	int status;

	if ((*s).state == S_LISTENING) {
		rx_available = 0;
		if (debug) console_print("Initialized %s connection in %s.\r\n",
			(*s).protocol,__func__);
		return 0;
	};

	rx_ready = TCPIP_TCP_GetIsReady((*s).socket);
	if (rx_ready>0) {
		TCPIP_TCP_ArrayGet((*s).socket,&rx_buffer[rx_available],rx_ready);	
		rx_available = rx_available+rx_ready;
	}
	if (rx_available == 0) return 0;
	if (debug) console_print("rx_ready=%u rx_available=%u in %s.\r\n",
		rx_ready, rx_available,__func__);
	if (rx_buffer[0] != START_CODE) {
		if (rx_buffer[0] == CLOSE_CODE) {
			if (debug) console_print("Close code received in %s.\r\n",__func__);
		} else {
			if (debug) console_print("Invalid start code in %s.\r\n",__func__);
		}
		return -1;
	}
	if (rx_available<9) return rx_available;		
	id = load32_be(&rx_buffer[1]);
	len = load32_be(&rx_buffer[5]);
	if (debug) console_print("id=%u len=%u in %s.\r\n",id,len,__func__);
	if (rx_available<len+10) return rx_available;
	if (rx_buffer[len+9] != END_CODE) {
		if (debug) console_print("Invalid end code in %s.\r\n",__func__);
		return -1;
	}
	status = lwdaq_process_message(s,id,len,&rx_buffer[9]);
	if (status<0) return status;
	if (rx_available>len+10) {
		if (debug) console_print("Copying %u to %u from %u in %s.\r\n",
			rx_available-len-10,0,len+10,__func__);
		memmove(&rx_buffer[0],&rx_buffer[len+10],rx_available-len-10);
		rx_available = rx_available-len-10;
	} else {
		rx_available = 0;
	}
	if (debug) console_print("rx_available=%u in %s.\r\n",rx_available,__func__);
	return rx_available;
}

/*
	telnet_tasks services a Telnet connection.
*/
int telnet_tasks(tcpip_server_type* s) {
//	static uint8_t rx_buffer[TCP_BUFF_SIZE];
    static char msg_buffer[TCP_BUFF_SIZE];
    int len;
	 
	if ((*s).state == S_LISTENING) {
		if (debug) console_print("Initialized %s connection in %s.\r\n",
			(*s).protocol,__func__);
		len = net_info(&msg_buffer[0],TCP_BUFF_SIZE);
		tcp_put((*s).socket,(const uint8_t*)&msg_buffer[0],len);
		return 0;
	};

/*
	len = TCPIP_TCP_GetIsReady((*s).socket);
	if (len>0) {
		TCPIP_TCP_ArrayGet((*s).socket,&rx_buffer[0],len);	
		tcp_put((*s).socket,&rx_buffer[0],len);
		if (debug) console_print("Echoed %u bytes in %s.\r\n",len,__func__);
		return len;
	}
	if (strcmp(cmd, "help") == 0)
	{
	do_help();
	}
	else if (strcmp(cmd, "netinfo") == 0)
	{
	do_netinfo();
	}
	else if (strcmp(cmd, "quit") == 0)
	{
	do_quit();
	}
	else if (strcmp(cmd, "reset") == 0)
	{
	do_reset();
	}
	else
	{
	console_print("Unknown command: %s\r\n", cmd);
	}*/
	return 0;
}

/*
	http_tasks services an HTTP connection.
*/
int http_tasks(tcpip_server_type* s) {
	int status = 0;
	 
	if (debug) console_print("Servicing %s connection on port %u in %s.\r\n",
		(*s).protocol,(*s).port,__func__);
	status = -1;

	return status;
}

int main ( void ) {
	int i = 0;
//	int j;

	// Initialize the clock, memory, core timers, system timer, UART console,
	// Ethernet physical interface, and the TCP/IP stack.
	pic_initialize();
	
	// Turn on the green D2 and the red D5 lamps.
   	pic_d2_on();
   	pic_d5_on();
 
   	// A while loop with a counter to control the state of our LEDs. 
	while (true) {
		tcpip_tick();
		console_server();
		tcpip_server(&lwdaq_server,lwdaq_tasks);
		tcpip_server(&telnet_server,telnet_tasks);
		tcpip_server(&http_server,http_tasks);
		
		// Flash the lights.
		i = i+1;
		if (i % 100 == 0) pic_d2_off();
		if (i % 1000 == 0) pic_d2_on();
		if (i == 200000) {
			pic_d5_toggle();
			i = 0;
		}
//		for (j = 0; j < 100; j++) {
//			j = bus_read_byte((uint8_t) i % 256);
//			bus_write_byte(0,j);
//		}
	}
	
	// Return an error code of the correct type if we get here.
	return (EXIT_FAILURE);
}


