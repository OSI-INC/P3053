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
#include "comms.h"
#include "console.h"

// Current version number
#define VERSION_NUM 15

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

// Variables and buffers for servers.
uint8_t lwdaq_rx_buff[TCP_BUFF_SIZE];
uint32_t lwdaq_rx_first = 0;
uint32_t lwdaq_rx_available = 0;

// Global socket variables.
#define LWDAQ_PORT 90
#define TELNET_PORT   23
#define HTTP_PORT 80

// Server state structures.
SERVER lwdaq_server = { INVALID_SOCKET, S_WAIT_STACK, LWDAQ_PORT, "LWDAQ" };
SERVER telnet_server = { INVALID_SOCKET, S_WAIT_STACK, TELNET_PORT, "TELNET" };
SERVER http_server = { INVALID_SOCKET, S_WAIT_STACK, HTTP_PORT, "HTTP" };

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
	sys_tick maintains the system and returns 1 if socket is still alive.
*/
void sys_tick(void) {
	DRV_MIIM_OBJECT_BASE_Default.DRV_MIIM_Tasks(sysObj.drvMiim_0);
	TCPIP_STACK_Task(sysObj.tcpip);
	console_server();
}

/*
	socket_tick maintains the system drivers and TCP/IP stack by calling
	sys_tick. It also checks the socket passed in as an argument and returns 1
	if the socket is still connected and zero otherwise.
*/
int socket_tick(TCP_SOCKET s) {
	sys_tick();
	if (!TCPIP_TCP_IsConnected(s) || TCPIP_TCP_WasDisconnected(s)) {
	 	return 0;
	} else {
		return 1;
	}
}

/*
	flip_bytes reverses the order of four bytes in a thirty-two bit variable, so
	as to convert little-endian to big-endian byte order, and visa-versa.
*/
int flip_bytes(uint32_t original) {
	uint8_t *a,*b;
	uint32_t result;
	a=((uint8_t *) &original);
	b=((uint8_t *) &result)+3;
	*b=*a;
	a=a+1;b=b+(-1);*b=*a;
	a=a+1;b=b+(-1);*b=*a;
	a=a+1;b=b+(-1);*b=*a;
	return result;
}

/*
	return_header sends a data return message header through a socket.
*/
int return_header(TCP_SOCKET s, uint32_t id, uint32_t len) {
	uint8_t buff[15];
	uint32_t* lp;
	
	buff[START_OFFSET]=START_CODE;
	lp=(uint32_t*)&buff[ID_OFFSET];
	*lp=flip_bytes(id);
	lp=(uint32_t*)&buff[CLEN_OFFSET];
	*lp=flip_bytes(len);
	TCPIP_TCP_ArrayPut(s,buff,CONTENT_OFFSET);
	return 0;
}

/*
	return_footer sends a terminating sequence through a socket.
*/
int return_footer(TCP_SOCKET s) {
	uint8_t buff[15];
	buff[0]=END_CODE;
	TCPIP_TCP_ArrayPut(s,buff,1);
	return 0;
}

/*
	return_byte sends a data return message through a socket with a single-byte
	integer as its content.
*/
int return_byte(TCP_SOCKET s, uint8_t data) {
	uint8_t buff[15];
	return_header(s,DATA_RETURN,sizeof(data));
	buff[0]=data;
	TCPIP_TCP_ArrayPut(s,buff,sizeof(data));
	return_footer(s);
	return 0;
}

/*
	return_int sends a data return message through a socket with a four-byte
	integer as its content.
*/
int return_int(TCP_SOCKET s, uint32_t data) {
	uint8_t buff[15];
	uint32_t* lp;

	return_header(s,DATA_RETURN,sizeof(data));
	lp=(uint32_t*)&buff[0];
	*lp=flip_bytes(data);
	TCPIP_TCP_ArrayPut(s,buff,sizeof(data));
	return_footer(s);
	return 0;
}

/*
	return_data sends a block of data through a socket.
*/
int return_data(TCP_SOCKET s, uint8_t* block, uint32_t len) {
	return_header(s,DATA_RETURN,len);
	TCPIP_TCP_ArrayPut(s,block,len);
	return_footer(s);
	return 0;
}

/*
	buffered_socket_read takes available bytes from the socket buffer and places
	them in a ram buffer so that we can execute commands consecutively without
	having to call the sock_fastread or sock_read routines. Each of these takes
	a couple of milliseconds to return, whether we read one byte or a thousand
	bytes. The routine returns the number of bytes it read. If the routine
	cannot supply the requested number of bytes, perhaps because the socket is
	closed or broken, it returns value -1. With the socket buffer empty, the
	routine calls socket_tick. If the socket is still open, the routine returns 0.
	If the socket is closed, broken, or if the ram buffer is overflowing with
	un-used data, the routine returns a value less than 0.
*/
int buffered_socket_read(TCP_SOCKET s, uint8_t* dp, uint32_t len) {
	uint32_t tcp_remaining;
	uint8_t* tcp_destination;

	if (lwdaq_rx_available>=len) {
		memcpy(dp,&lwdaq_rx_buff[lwdaq_rx_first],len);
		lwdaq_rx_first=lwdaq_rx_first+len;
		lwdaq_rx_available=lwdaq_rx_available-len;
	} else {
		tcp_remaining=len;
		tcp_destination=dp;
		if (lwdaq_rx_available>0) {
			memcpy(tcp_destination,&lwdaq_rx_buff[lwdaq_rx_first],lwdaq_rx_available);
			tcp_remaining=tcp_remaining-lwdaq_rx_available;
			tcp_destination=tcp_destination+lwdaq_rx_available;
			console_print("Read last %d available, need %d more in %s.\r\n",
				lwdaq_rx_available,tcp_remaining,__func__);
		}
		lwdaq_rx_available=0;
		lwdaq_rx_first=0;
		while (tcp_remaining>0) {
			console_print("Waiting for %d bytes in %s.\r\n",tcp_remaining,__func__);
			while (lwdaq_rx_available==0) {
				lwdaq_rx_available=TCPIP_TCP_GetIsReady(s);
				if (lwdaq_rx_available>0) {
					TCPIP_TCP_ArrayGet(s,lwdaq_rx_buff,lwdaq_rx_available);	
				} 
				if (!socket_tick(s)) {
					console_print("Socket closed by client in %s.\r\n",__func__);
 					return -1;
				}
			}
			console_print("Received %d bytes in %s.\r\n",lwdaq_rx_available,__func__);
			if (lwdaq_rx_available>=tcp_remaining) {
				memcpy(tcp_destination,lwdaq_rx_buff,tcp_remaining);
				lwdaq_rx_first=tcp_remaining;
				lwdaq_rx_available=lwdaq_rx_available-tcp_remaining;
				tcp_remaining=0;
			} else {
				memcpy(tcp_destination,lwdaq_rx_buff,lwdaq_rx_available);
				tcp_destination=tcp_destination+lwdaq_rx_available;
				tcp_remaining=tcp_remaining-lwdaq_rx_available;
				lwdaq_rx_available=0;
			}
		}
	}
	return len;
}

/*
	receive_message reads data from a socket until an entire message has been
	received. It saves the message id to *id and the content to *content. It
	reports the length of the content in *len. The routine assumes the LWDAQ
	Message Protocol.
*/
int receive_message(TCP_SOCKET s, uint32_t* id, uint32_t* len, uint8_t* content) {
	uint8_t code;

	// We read the start code. If it's incorrect, we close the socket and return
	// with an error code.
	if (buffered_socket_read(s,&code,sizeof(code)) < 0) {
		TCPIP_TCP_Close(s);
		return -1;
	}
	if (code!=START_CODE) {
		if (code==CLOSE_CODE) {
			console_print("Close code received, closing socket in %s.\r\n",__func__);
		} else {
			console_print("Invalid start code, closing socket in %s.\r\n",__func__);
		}
		TCPIP_TCP_Close(s);
		return -1;
	}
	
	// We read the message identifier and content length.
	if (buffered_socket_read(s,(uint8_t*)id,sizeof(*id)) < 0) {
		TCPIP_TCP_Close(s);
		return -1;
	}
	if (buffered_socket_read(s,(uint8_t*)len,sizeof(*len)) < 0) {
		TCPIP_TCP_Close(s);
		return -1;
	}
	
	// We flip the big-endian integer bytes around to make them little-endian.
	*id=flip_bytes(*id);
	*len=flip_bytes(*len);
	
	// We are limited in the size of a single message by the length of our input
	// buffer, so we check to see if the contents length specified in the
	// message header exceeds the length of our buffer. If so, we close the
	// socket.
	if (*len>BUFF_SIZE) {
		console_print("Message too long, closing socket in %s.\r\n",__func__);
		TCPIP_TCP_Close(s);
		return -1;
	}
	
	// We read the content itself into a buffer. At the end of the content we
	// write a null character so we can pass the content buffer to
	// string-handling routines.
	if (*len>0) {
		if (buffered_socket_read(s,content,(int) *len) < 0) {
			TCPIP_TCP_Close(s);
			return -1;
		}
		content[(int) *len]=0x00;
	} else {
		content[0]=0x00;
	}
	
	// Read the end code.
	if (buffered_socket_read(s,&code,sizeof(code)) < 0) {
		TCPIP_TCP_Close(s);
		return -1;
	}
	if (code!=END_CODE) {
		console_print("Invalid end code, closing socket in %s.\r\n",__func__);
		TCPIP_TCP_Close(s);
		return -1;
	}
	
	return 0;
}

/*
	process_message handles an incoming LWDAQ message.
*/
int process_message (TCP_SOCKET s, uint32_t id, uint32_t len, uint8_t* content) {
	uint8_t register_addr = 0;
	uint8_t value = 0;
//	int status;
	
	switch (id) {
		case BYTE_WRITE:{
			register_addr=content[3];
			value=content[4];
			//write_controller_byte(register_addr,value);
			console_print("BYTE_WRITE to %d of %d in %s.\r\n",
				register_addr,value,__func__);
			break;
		}

		case BYTE_READ:{
			register_addr=content[3];
			//value=read_controller_byte(register_addr);
			console_print("BYTE_READ from %d of %d in %s.\r\n",
				register_addr,value,__func__);
			//return_byte(value);
			break;
		}

		case BYTE_POLL:{
			register_addr=content[3];
			value=content[4];
			console_print("BYTE_POLL of %d for %d in %s.\r\n",
				register_addr,value,__func__);
/*
			while (read_controller_byte(register_addr) != value) {
				status=buffered_socket_read(&socket,NULL,0);
				if (status<0) break;
			}
*/
			break;
		}

		case VERSION_READ:{
			console_print("VERSION_READ in %s.\r\n",__func__);
			return_int(s,VERSION_NUM);
			break;
		}

		case ECHO:{
			console_print("ECHO of %u bytes in %s.\r\n",len,__func__);
			return_data(s,content,len);
			content[len]=0x00;
			console_print("%s\r\n",content);
			break;
		}

		default: {
			console_print("Unrecognised message in %s.\r\n",__func__);
			break;
		}
	}
	return 0;
}

/*
	lwdaq_tasks services a LWDAQ socket connection.
*/
int lwdaq_tasks(SERVER* s) {
	uint32_t id,len;
	uint8_t* content;
	int status;
	
	if ((*s).state == S_LISTENING) {
		lwdaq_rx_first = 0;
		lwdaq_rx_available = 0;
		console_print("Initialized %s connection in %s.\r\n",(*s).protocol,__func__);
		return 0;
	};

	if ((*s).state == S_SERVING) {
		console_print("Receiving %s message in %s.\r\n",(*s).protocol,__func__);
		status=receive_message((*s).socket,&id,&len,content);
		if (status >= 0) {
			console_print("Received: id=%u len=%u in %s.\r\n",id,len,__func__);
			return len;
		} else {
			return status;
		}
	};
	
	return -1;
}

/*
	telnet_tasks services a Telnet socket connection.
*/
int telnet_tasks(SERVER* s) {
	int status = 0;
	 
	console_print("Servicing %s connection on port %u by closing in %s.\r\n",
		(*s).protocol,(*s).port,__func__);
	status = -1;

	return status;
}

/*
	http_tasks services an HTTP socket connection.
*/
int http_tasks(SERVER* s) {
	int status = 0;
	 
	console_print("Servicing %s connection on port %u by closing in %s.\r\n",
		(*s).protocol,(*s).port,__func__);
	status = -1;

	return status;
}


int main ( void ) {
	int i;

	// Disable interrupts for initialization.
	(void)__builtin_disable_interrupts();	
	
	CLK_Initialize();
	ACCESS_Initialize();
	pic_initialize();
	NVM_Initialize();
	CORETIMER_Initialize();
	UART2_Initialize();
	UART_SERIAL_SETUP uart2Setup = {
		.baudRate = 115200,
		.dataWidth = UART_DATA_8_BIT,
		.parity = UART_PARITY_NONE,
		.stopBits = UART_STOP_1_BIT
	};
	UART2_SerialSetup(&uart2Setup, 0);
	UTILS_Initialize();
	console_initialize();
	TCPIP_Initialize();
	
	// Re-enable interrupts and report initialization complete.
	(void)__builtin_enable_interrupts();
	console_print("Console initialized, systems starting up in %s.\r\n",__func__);
	
	// Turn on the red and green lamps.
   	GPIO_PortSet(GPIO_PORT_A,0x00000004);
   	GPIO_PortSet(GPIO_PORT_C,0x00008000);
   	
   	// A while loop with a counter to control the state of our LEDs. 
   	i=0;
	while (true) {
		sys_tick();
		tcpip_server(&lwdaq_server,lwdaq_tasks);
		tcpip_server(&telnet_server,telnet_tasks);
		tcpip_server(&http_server,http_tasks);
		
		i = i+1;
		if (i % 100 == 0) {
			GPIO_PortClear(GPIO_PORT_C,0x00008000);
		}
		if (i % 1000 == 0) {
			GPIO_PortSet(GPIO_PORT_C,0x00008000);
		}
		if (i==200000) {
			GPIO_PortToggle(GPIO_PORT_A,0x00000004);
			i=0;
		}
	}
	
	// Return an error code of the correct type if we get here.
	return (EXIT_FAILURE);
}


