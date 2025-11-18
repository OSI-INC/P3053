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

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>                  
#include <stdbool.h>
#include <stdlib.h>
#include <stdarg.h>
#include "configuration.h"
#include "definitions.h"
#include "console.h"
#include "utils.h"

// Configuration constants.
#define TCPIP_SERVER_PORT 90
#define ETH_MTU 1514
#define BUFF_SIZE (ETH_MTU-40)
#define RAM_BUFF_SIZE (6*BUFF_SIZE)
#define TCP_BUFF_SIZE (6*BUFF_SIZE)
#define CHECK_TCP_COUNT 1000
#define CONFIG_LENGTH 1024 // bytes for config file buffer
#define SEPCHARS " :\n,;=" // separator characters in config file

// Global variables.
char logged_in;
int ip_port,tcp_timeout,security_level;
char password[32];
char configuration[CONFIG_LENGTH];
uint8_t tcp_buffer[TCP_BUFF_SIZE];
int tcp_first,tcp_available;

// LWDAQ messages
#define START_CODE 0xA5 // correct value for start code
#define CLOSE_CODE 0x04 // close connection character
#define END_CODE 0x5A // correct value for end code
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

// Data acquisition state names and variable. The TCP_SOCKET type is defined as
// a sixteen-bit signed integer in tcp.h.
typedef enum {
	DAQ_TCPIP_WAIT_INIT,
	DAQ_TCPIP_WAIT_FOR_IP,	
	DAQ_TCPIP_OPENING_SERVER,
	DAQ_TCPIP_WAIT_FOR_CONNECTION,
	DAQ_TCPIP_SERVING_CONNECTION,
	DAQ_TCPIP_CLOSING_CONNECTION,
	DAQ_TCPIP_ERROR,
} daq_states;
typedef struct {
    daq_states state;
    TCP_SOCKET socket;
} daq_data_type;
daq_data_type daq_data;


/*
	tcp_tick maintains the system and returns 1 if socket is still alive.
*/
int tcp_tick(void) {
	DRV_MIIM_OBJECT_BASE_Default.DRV_MIIM_Tasks(sysObj.drvMiim_0);
	TCPIP_STACK_Task(sysObj.tcpip);
	CMD_Tasks();
    if (!TCPIP_TCP_IsConnected(daq_data.socket) 
        || TCPIP_TCP_WasDisconnected(daq_data.socket)) {
     	return 0;
    } else {
    	return 1;
    }
}

/*
flip_bytes reverses the order of four bytes in a thirty-two bit
variable, so as to convert little-endian to big-endian byte order,
and visa-versa.
*/
int flip_bytes(int original) {
	char *a,*b;
	int result;
	a=((char *) &original);
	b=((char *) &result)+3;
	*b=*a;
	a=a+1;b=b+(-1);*b=*a;
	a=a+1;b=b+(-1);*b=*a;
	a=a+1;b=b+(-1);*b=*a;
	return result;
}

/*
	buffered_socket_read takes available bytes from the socket buffer and places
	them in a ram buffer so that we can execute commands consecutively without
	having to call the sock_fastread or sock_read routines. Each of these takes
	a couple of milliseconds to return, whether we read one byte or a thousand
	bytes. The routine returns the number of bytes it read. If the routine
	cannot supply the requested number of bytes, perhaps because the socket is
	closed or broken, it returns value -1. With the socket buffer empty, the
	routine calls tcp_tick. If the socket is still open, the routine returns 0.
	If the socket is closed, broken, or if the ram buffer is overflowing with
	un-used data, the routine returns a value less than 0.
*/
int buffered_socket_read(uint8_t* dp, int len) {
	int tcp_remaining;
	uint8_t* tcp_destination;

	if (tcp_available>=len) {
		memcpy(dp,&tcp_buffer[tcp_first],len);
		tcp_first=tcp_first+len;
		tcp_available=tcp_available-len;
	} else {
		tcp_remaining=len;
		tcp_destination=dp;
		if (tcp_available>0) {
			memcpy(tcp_destination,&tcp_buffer[tcp_first],tcp_available);
			tcp_remaining=tcp_remaining-tcp_available;
			tcp_destination=tcp_destination+tcp_available;
			console_print("Read last %d available, need %d more.\n",
				tcp_available,tcp_remaining);
		}
		tcp_available=0;
		tcp_first=-1;
		while (tcp_remaining>0) {
			console_print("Waiting for %d bytes...\r\n",tcp_remaining);
			while (tcp_available==0) {
				tcp_available=TCPIP_TCP_GetIsReady(daq_data.socket);
				if (tcp_available>0) {
					TCPIP_TCP_ArrayGet(daq_data.socket,tcp_buffer,tcp_available);	
				} 
				if (!tcp_tick()) {
	                console_print("Connection broken.\r\n");
 					return -1;
				}
			}
			console_print("Received %d bytes.\r\n",tcp_available);
			if (tcp_available>=tcp_remaining) {
				memcpy(tcp_destination,tcp_buffer,tcp_remaining);
				tcp_first=tcp_remaining;
				tcp_available=tcp_available-tcp_remaining;
				tcp_remaining=0;
			} else {
				memcpy(tcp_destination,tcp_buffer,tcp_available);
				tcp_destination=tcp_destination+tcp_available;
				tcp_remaining=tcp_remaining-tcp_available;
				tcp_available=0;
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
int receive_message(int* id, int* len, uint8_t* content) {
	uint8_t code;

	// We read the start code. If it's incorrect, we close the socket and return
	// with an error code.
	buffered_socket_read(&code,sizeof(code));
	if (code!=START_CODE) {
		if (code==CLOSE_CODE) {
			console_print("Close code received, closing socket.\r\n");
		} else {
			console_print("Invalid start code, closing socket.\r\n");
		}
		TCPIP_TCP_Close(daq_data.socket);
		return -1;
	}
	
	// We read the message identifier and content length.
	buffered_socket_read((uint8_t*)id,sizeof(*id));
	buffered_socket_read((uint8_t*)len,sizeof(*len));
	
	// We flip the big-endian integer bytes around to make them little-endian.
	*id=flip_bytes(*id);
	*len=flip_bytes(*len);
	
	// We are limited in the size of a single message by the length of our input
	// buffer, so we check to see if the contents length specified in the
	// message header exceeds the length of our buffer. If so, we close the
	// socket.
	if (*len>BUFF_SIZE) {
		console_print("Message content too long, closing socket.\n");
		TCPIP_TCP_Close(daq_data.socket);
		return -1;
	}
	
	// We read the content itself into a buffer. At the end of the content we
	// write a null character so we can pass the content buffer to
	// string-handling routines.
	if (*len>0) {
	buffered_socket_read(content,(int) *len);
		content[(int) *len]=0x00;
	} else {
		content[0]=0x00;
	}
	
	// Read the end code.
	buffered_socket_read(&code,sizeof(code));
	if (code!=END_CODE) {
		console_print("Invalid end code, closing socket.\n");
		TCPIP_TCP_Close(daq_data.socket);
		return -1;
	}
	
	return 0;
}

/*
	General-Purpose Input-Output Initialization.
*/
void hardware_configure (void)
{
	// The A3053A is all-digital. so we configure all pins as digital pins. We
	// don't even bother to check the data sheet to see which pins can be
	// non-digital, we just set them all to digital even if they are always
	// digital.
	ANSELA = 0x00000000;
	ANSELB = 0x00000000;
	ANSELC = 0x00000000;
	ANSELD = 0x00000000;
	ANSELE = 0x00000000;
	ANSELF = 0x00000000;
	ANSELG = 0x00000000;
	
    // We unlock access to the configuration registers by writing a sequence of
    // three values to the SYSKEY register. These three key values work on all
    // PIC32 microprocessors. 
    SYSKEY = 0x00000000U;
    SYSKEY = 0xAA996655U;
    SYSKEY = 0x556699AAU;
    
    // Now that we have unlocked the configuration registers for writing, we
    // write to the IOLOCK bit of the configuration control register to enable
    // writing to the Peripheral Pin Selection (PPS) registers.
    CFGCONbits.IOLOCK = 0U;

	// Select RF8 as the source of UART2 RX. On the A3053A, RF8 is U1-58,
	// connected to R11, which in turn feeds D4, the white test point LED.
	U2RXR = 0b1011;
	
	// Select UART2 TX as the source of RF2. On the A3053A, RF2 is U1-57,
	// connected to, R10, which in turn feeds D3, the blue test point LED.
	RPF2R = 0b0010;
	
    // Lock the PPS registers.
    CFGCONbits.IOLOCK = 1U;
    
    // Lock the configuration registers.
    SYSKEY = 0x00000000U;
    
    // So far, on our A3053A, we have have D3 and D4 dedicated to UART2, but D2
    // and D5 are available as test points. Pin U1-56 is RF3, so we want to set
    // bit 3 of port F as an output. Pin U1-59 is RA2, so we want to set bit 2
    // of port A as an output as well. The constants that hold the numerical
    // port codes are defined in plib_gpio.h. To specify the bit, we provide a
    // mask.
   	GPIO_PortOutputEnable(GPIO_PORT_F,0x00000008);
   	GPIO_PortOutputEnable(GPIO_PORT_A,0x00000004);
   	GPIO_PortOutputEnable(GPIO_PORT_C,0x00008000);
}

/*
	DAQ_Tasks handles TCP/IP connections. Right now the code accepts a telnet
	socket connection and prints what it receives from the socket to the
	console. It responds to ping too.
*/
void DAQ_Tasks (void) 
{
    SYS_STATUS          tcpip_status;
    const char          *interface_name, *host_name;
    IPV4_ADDR           ip_addr;
    TCP_SOCKET_INFO		sock_info;
    TCPIP_NET_HANDLE    net_hdl;
	static uint8_t 		in_buffer[BUFF_SIZE];
	int 				id, len;

    switch (daq_data.state) {
        case DAQ_TCPIP_WAIT_INIT: {
            tcpip_status = TCPIP_STACK_Status(sysObj.tcpip);
            if (tcpip_status < 0) {   
                console_print("TCP/IP stack initialization failed.\r\n");
                daq_data.state = DAQ_TCPIP_ERROR;
            } else if (tcpip_status == SYS_STATUS_READY) {
				net_hdl = TCPIP_STACK_IndexToNet(0);
				interface_name = TCPIP_STACK_NetNameGet(net_hdl);
				host_name = TCPIP_STACK_NetBIOSName(net_hdl);
				console_print(
					"Interface %s on host %s awaiting initialization.\r\n",
					interface_name,
					string_trim(host_name));
                daq_data.state = DAQ_TCPIP_WAIT_FOR_IP;
            }
        }
        break;

        case DAQ_TCPIP_WAIT_FOR_IP: {
			net_hdl = TCPIP_STACK_IndexToNet(0);
			if(!TCPIP_STACK_NetIsReady(net_hdl)) {
				return;
			}
			ip_addr.Val = TCPIP_STACK_NetAddress(net_hdl);
			interface_name = TCPIP_STACK_NetNameGet(net_hdl);
			 console_print(
				"Interface %s assigned IP address %d.%d.%d.%d.\r\n", 
				interface_name,
				ip_addr.v[0], 
				ip_addr.v[1],
				ip_addr.v[2],
				ip_addr.v[3]);
			daq_data.state = DAQ_TCPIP_OPENING_SERVER;
         	force_gateway_arp();
        	ping_gateway();
       }
        break;
            
        case DAQ_TCPIP_OPENING_SERVER: {
            console_print("Waiting for connection on port %d.\r\n",
            	TCPIP_SERVER_PORT);
            daq_data.socket = TCPIP_TCP_ServerOpen(
            	IP_ADDRESS_TYPE_IPV4, 
            	TCPIP_SERVER_PORT, 0);
            if (daq_data.socket == INVALID_SOCKET) {
                console_print("Could not open server socket.\r\n");
                break;
            }
            daq_data.state = DAQ_TCPIP_WAIT_FOR_CONNECTION;
        }
        break;

        case DAQ_TCPIP_WAIT_FOR_CONNECTION: {
            if (!TCPIP_TCP_IsConnected(daq_data.socket)) {
                return;
            } else {
				if(TCPIP_TCP_SocketInfoGet(daq_data.socket, &sock_info)) {
					IPV4_ADDR ip = sock_info.remoteIPaddress.v4Add;
					console_print("Received connection from %u.%u.%u.%u\r\n",
						ip.v[0], ip.v[1], ip.v[2], ip.v[3]);
				} else {
					console_print("Received connection, unknown peer.");
				}
				daq_data.state = DAQ_TCPIP_SERVING_CONNECTION;
            }
        }
        break;

        case DAQ_TCPIP_SERVING_CONNECTION: {
            if (!TCPIP_TCP_IsConnected(daq_data.socket) 
            	|| TCPIP_TCP_WasDisconnected(daq_data.socket)) {
                daq_data.state = DAQ_TCPIP_CLOSING_CONNECTION;
                console_print("Connection closed.\r\n");
                break;
            }
            
            if (receive_message(&id,&len,in_buffer)<0) {
            	daq_data.state = DAQ_TCPIP_CLOSING_CONNECTION;
            	break;
            }
            
            console_print("id=%u, len=%u, %s\r\n",id,len,in_buffer);
        }
        break;
        
        case DAQ_TCPIP_CLOSING_CONNECTION: {
            TCPIP_TCP_Close(daq_data.socket);
            daq_data.socket = INVALID_SOCKET;
            daq_data.state = DAQ_TCPIP_OPENING_SERVER;
        }
        break;
        
        default:
        break;
    }
}

int main ( void )
{
	int i;

	// Disable interrupts for initialization.
	(void)__builtin_disable_interrupts();	
	
	CLK_Initialize();
	ACCESS_Initialize();
	hardware_configure();
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
	CMD_Initialize();
	TCPIP_Initialize();
	daq_data.state = DAQ_TCPIP_WAIT_INIT;
	
	// Re-enable interrupts and report initialization complete.
	(void)__builtin_enable_interrupts();
	console_print("Initialization complete, systems starting up.\r\n");
	
	// Turn on the red and green lamps.
   	GPIO_PortSet(GPIO_PORT_A,0x00000004);
   	GPIO_PortSet(GPIO_PORT_C,0x00008000);
   	
   	// A while loop with a counter to control the state of our LEDs. 
   	i=0;
    while (true) {
		// Maintain the RMII interface with the PHY.
		DRV_MIIM_OBJECT_BASE_Default.DRV_MIIM_Tasks(sysObj.drvMiim_0);
	
		// Maintain the TCP/IP stack.
		TCPIP_STACK_Task(sysObj.tcpip);
	
		// Maintain our own data acquisition state machine.
		DAQ_Tasks();
		CMD_Tasks();
		
		// Maintain the indicator lamps.
		i = i+1;
		if (i % 100 == 0)
		{
			GPIO_PortClear(GPIO_PORT_C,0x00008000);
		}
		if (i % 1000 == 0)
		{
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


