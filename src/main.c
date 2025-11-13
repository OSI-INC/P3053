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

/*
	Configuration constants.
*/
#define TCPIP_SERVER_PORT 90


/*
	General-Purpose Input-Output Initialization.
*/
void GPIO_Initialize (void)
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
	Define state names for our data acquisition (DAQ) state machine. 
*/
typedef enum
{
	DAQ_TCPIP_WAIT_INIT,
	DAQ_TCPIP_WAIT_FOR_IP,	
	DAQ_TCPIP_OPENING_SERVER,
	DAQ_TCPIP_WAIT_FOR_CONNECTION,
	DAQ_TCPIP_SERVING_CONNECTION,
	DAQ_TCPIP_CLOSING_CONNECTION,
	DAQ_TCPIP_ERROR,
} daq_states;

/*
	Define a varaiable that contains DAQ state, as enumerated above, and a single
	socket that the state machine is dealing with. The TCP_SOCKET type is defined
	as a sixteen-bit signed integer in tcp.h.
*/
typedef struct
{
    daq_states state;
    TCP_SOCKET socket;
} daq_data_type;
daq_data_type daq_data;

/*
	DAQ_Tasks is where we handle TCP/IP connections. Right now it's an echo server
	that handles only one socket at a time.
*/
void DAQ_Tasks (void) 
{
    SYS_STATUS          tcpipStat;
    const char          *netName, *netBiosName;
    static IPV4_ADDR    dwLastIP[2] = { {-1}, {-1} };
    IPV4_ADDR           ipAddr;
    int                 i, nNets;
    TCPIP_NET_HANDLE    netH;

    switch (daq_data.state)
    {
        case DAQ_TCPIP_WAIT_INIT:
        {
            tcpipStat = TCPIP_STACK_Status(sysObj.tcpip);
            if (tcpipStat < 0) {   
                console_printf("TCP/IP stack initialization failed.\r\n");
                daq_data.state = DAQ_TCPIP_ERROR;
            } else if (tcpipStat == SYS_STATUS_READY) {
                nNets = TCPIP_STACK_NumberOfNetworksGet();
                for(i = 0; i < nNets; i++) {
                    netH = TCPIP_STACK_IndexToNet(i);
                    netName = TCPIP_STACK_NetNameGet(netH);
                    netBiosName = TCPIP_STACK_NetBIOSName(netH);
                    console_printf(
                    	"Interface %s on host %s awaiting initialization.\r\n",
                    	netName,
                    	netBiosName);
                }
                daq_data.state = DAQ_TCPIP_WAIT_FOR_IP;
            }
        }
        break;

        case DAQ_TCPIP_WAIT_FOR_IP:
        {
            nNets = TCPIP_STACK_NumberOfNetworksGet();
            for (i = 0; i < nNets; i++) {
                netH = TCPIP_STACK_IndexToNet(i);
                if(!TCPIP_STACK_NetIsReady(netH)) {
                    return;
                }
                ipAddr.Val = TCPIP_STACK_NetAddress(netH);
                if (dwLastIP[i].Val != ipAddr.Val) {
                    dwLastIP[i].Val = ipAddr.Val;
                     console_printf(
                    	"Interface %s assigned IP address %d.%d.%d.%d.\r\n", 
                    	TCPIP_STACK_NetNameGet(netH),
                    	ipAddr.v[0], 
                    	ipAddr.v[1],
                    	ipAddr.v[2],
                    	ipAddr.v[3]);
                }
                daq_data.state = DAQ_TCPIP_OPENING_SERVER;
            }
        }
        break;
            
        case DAQ_TCPIP_OPENING_SERVER:
        {
            console_printf("Waiting for connection on port %d.\r\n",
            	TCPIP_SERVER_PORT);
            daq_data.socket = TCPIP_TCP_ServerOpen(
            	IP_ADDRESS_TYPE_IPV4, 
            	TCPIP_SERVER_PORT, 0);
            if (daq_data.socket == INVALID_SOCKET) {
                console_printf("Could not open server socket.\r\n");
                break;
            }
            daq_data.state = DAQ_TCPIP_WAIT_FOR_CONNECTION;
        }
        break;

        case DAQ_TCPIP_WAIT_FOR_CONNECTION:
        {
            if (!TCPIP_TCP_IsConnected(daq_data.socket)) {
                return;
            } else {
                daq_data.state = DAQ_TCPIP_SERVING_CONNECTION;
                console_printf("Received connection.\r\n");
            }
        }
        break;

        case DAQ_TCPIP_SERVING_CONNECTION:
        {
            if (!TCPIP_TCP_IsConnected(daq_data.socket) 
            	|| TCPIP_TCP_WasDisconnected(daq_data.socket)) {
                daq_data.state = DAQ_TCPIP_CLOSING_CONNECTION;
                console_printf("Connection closed.\r\n");
                break;
            }
            int16_t wMaxGet, wMaxPut, wCurrentChunk;
            uint16_t w, w2;
            uint8_t AppBuffer[32 + 1];
            
            // Figure out how many bytes have been received and how many we can
            // transmit. The GetIsReady function returns the number of bytes
            // available to read, while the PutIsReady function returns how many
            // bytes we can write to the output buffer before it is full.
            wMaxGet = TCPIP_TCP_GetIsReady(daq_data.socket);
            wMaxPut = TCPIP_TCP_PutIsReady(daq_data.socket);

            // Make sure we don't take more bytes out of the RX FIFO than we can
            // put into the TX FIFO
            if (wMaxPut < wMaxGet) {
                wMaxGet = wMaxPut;
            }

            // Process all bytes that we can. This is implemented as a loop,
            // processing up to sizeof(AppBuffer) bytes at a time. This limits
            // memory usage while maximizing performance.  Single byte Gets and
            // Puts are a lot slower than multibyte GetArrays and PutArrays.
            wCurrentChunk = sizeof(AppBuffer) -1;
            for (w = 0; w < wMaxGet; w += sizeof(AppBuffer) - 1) {
				// Make sure the last chunk, which will likely be smaller than
				// our buffer, is treated correctly.
				if (w + sizeof(AppBuffer) - 1 > wMaxGet) wCurrentChunk = wMaxGet - w;

                // Transfer the data out of the TCP RX FIFO and into our local
                // processing buffer.
                TCPIP_TCP_ArrayGet(daq_data.socket, AppBuffer, wCurrentChunk);

                // Perform the "ToUpper" operation on each data byte
                for (w2 = 0; w2 < wCurrentChunk; w2++) {
                    i = AppBuffer[w2];
                    if (i == '\x1b') {
                        daq_data.state = DAQ_TCPIP_CLOSING_CONNECTION;
                        console_printf("Connection closed.\r\n");
                    }
                }
                AppBuffer[w2] = 0;  // end the console string properly

                // Transfer the data out of our local processing buffer and into
                // the TCP TX FIFO.
                console_printf("Transmit: %s\r\n", AppBuffer);
                TCPIP_TCP_ArrayPut(daq_data.socket, AppBuffer, wCurrentChunk);

                // No need to perform any flush. TCP data in TX FIFO will
                // automatically transmit itself after it accumulates for a
                // while. If you want to decrease latency at the expense of
                // wasting network bandwidth on TCP overhead, perform an
                // explicit flush via the TCPFlush() API.
            }
        }
        break;
        
        case DAQ_TCPIP_CLOSING_CONNECTION:
        {
            TCPIP_TCP_Close(daq_data.socket);
            daq_data.socket = INVALID_SOCKET;
            daq_data.state = DAQ_TCPIP_WAIT_FOR_IP;
        }
        break;
        
        default:
        break;
    }
}


/*
	SYS_Initialize performs a sequence of functions necessary when the CPU boots
	up. It calls routines defined in initialize.c and elsewhere in the
	configuration source code. The CLK routine intializes the clock. The MA
	routine initializes memory access: wait states and error code correction.
	The GPIO routine configures the MCU pins, including peripheral pin selection
	(PPS). The NVM routine initializes the non-volatile flash memory. The
	CORETIMER routine we have yet to investigate. We initialize the UART2
	interface directly in the routine. The TCPIP routine initializes the TCPIP
	stack.
*/
void SYS_Initialize (void* data)
{
	(void)__builtin_disable_interrupts();	
	CLK_Initialize();
	ACCESS_Initialize();
	GPIO_Initialize();
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
	TCPIP_Initialize();
	daq_data.state = DAQ_TCPIP_WAIT_INIT;
	(void)__builtin_enable_interrupts();
}

int main ( void )
{
	int i;

	// Call the system initialization routine, which is defined above.
	SYS_Initialize(NULL);
	
	// Print messages now that our UART console is ready.
	console_printf("\r\n");
	console_printf("===================================================\r\n");
	console_printf("System initialization routine has completed.\r\n");

	// Turn on the red and green lamps.
   	GPIO_PortSet(GPIO_PORT_A,0x00000004);
   	GPIO_PortSet(GPIO_PORT_C,0x00008000);
   	
   	// A while loop with a counter to control the state of our LEDs. 
   	i=0;
    while (true) {
		// Attend to the system command console.
		SYS_CMD_Tasks();
	
		// Maintain the RMII interface with the PHY.
		DRV_MIIM_OBJECT_BASE_Default.DRV_MIIM_Tasks(sysObj.drvMiim_0);
	
		// Maintain the TCP/IP stack.
		TCPIP_STACK_Task(sysObj.tcpip);
	
		// Maintain our own data acquisition state machine.
		DAQ_Tasks();
		
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
