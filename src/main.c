/*******************************************************************************
  Main Source File

  Company:
    Microchip Technology Inc.

  File Name:
    main.c

  Summary:
    This file contains the "main" function for a project.

  Description:
    This file contains the "main" function for a project.  The
    "main" function calls the "SYS_Initialize" function to initialize the state
    machines of all modules in the system
 *******************************************************************************/

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>                  
#include <stdbool.h>
#include <stdlib.h>
#include "configuration.h"
#include "definitions.h"
#include "tcpip/tcpip.h"

#define SERVER_PORT 90

typedef enum
{
	APP_TCPIP_WAIT_INIT,
	APP_TCPIP_WAIT_FOR_IP,	
	APP_TCPIP_OPENING_SERVER,
	APP_TCPIP_WAIT_FOR_CONNECTION,
	APP_TCPIP_SERVING_CONNECTION,
	APP_TCPIP_CLOSING_CONNECTION,
	APP_TCPIP_ERROR,
} APP_STATES;

typedef struct
{
    APP_STATES state;
    TCP_SOCKET socket;
} APP_DATA;

APP_DATA appData;

/*
	APP_Tasks is where we handle TCP/IP connections. Right now it is supposed to be
	an echo server, but it's not working yet. The code is a direct copy from the
	echo server example in Harmony3.
*/
void APP_Tasks ( void )
{
    SYS_STATUS          tcpipStat;
    const char          *netName, *netBiosName;
    static IPV4_ADDR    dwLastIP[2] = { {-1}, {-1} };
    IPV4_ADDR           ipAddr;
    int                 i, nNets;
    TCPIP_NET_HANDLE    netH;

    switch (appData.state)
    {
        case APP_TCPIP_WAIT_INIT:
            tcpipStat = TCPIP_STACK_Status(sysObj.tcpip);
            if (tcpipStat < 0)
            {   
            	// Some error occurred
                SYS_CONSOLE_MESSAGE(" APP: TCP/IP stack initialization failed!\r\n");
                appData.state = APP_TCPIP_ERROR;
            }
            else if(tcpipStat == SYS_STATUS_READY)
            {
                // Now that the stack is ready we can check the
                // available interfaces
                nNets = TCPIP_STACK_NumberOfNetworksGet();
                for(i = 0; i < nNets; i++)
                {

                    netH = TCPIP_STACK_IndexToNet(i);
                    netName = TCPIP_STACK_NetNameGet(netH);
                    netBiosName = TCPIP_STACK_NetBIOSName(netH);

#if defined(TCPIP_STACK_USE_NBNS)
                    SYS_CONSOLE_PRINT("    Interface %s on host %s - NBNS enabled\r\n", 
                    	netName, netBiosName);
#else
                    SYS_CONSOLE_PRINT("    Interface %s on host %s - NBNS disabled\r\n", 
                    	netName, netBiosName);
#endif 
                    (void)netName;          // avoid compiler warning 
                    (void)netBiosName;      // if SYS_CONSOLE_PRINT is null macro

                }
                appData.state = APP_TCPIP_WAIT_FOR_IP;

            }
            break;

        case APP_TCPIP_WAIT_FOR_IP:

            // If the IP address of an interface has changed
            // display the new value on the system console.
            nNets = TCPIP_STACK_NumberOfNetworksGet();

            for (i = 0; i < nNets; i++)
            {
                netH = TCPIP_STACK_IndexToNet(i);
                if(!TCPIP_STACK_NetIsReady(netH))
                {
                    return;
                }
                ipAddr.Val = TCPIP_STACK_NetAddress(netH);
                if (dwLastIP[i].Val != ipAddr.Val)
                {
                    dwLastIP[i].Val = ipAddr.Val;

                    SYS_CONSOLE_MESSAGE(TCPIP_STACK_NetNameGet(netH));
                    SYS_CONSOLE_MESSAGE(" IP Address: ");
                    SYS_CONSOLE_PRINT("%d.%d.%d.%d \r\n", ipAddr.v[0], 
                    	ipAddr.v[1], ipAddr.v[2], ipAddr.v[3]);
                }
                appData.state = APP_TCPIP_OPENING_SERVER;
            }
            break;
            
        case APP_TCPIP_OPENING_SERVER:
        {
            SYS_CONSOLE_PRINT("Waiting for Client Connection on port: %d\r\n", SERVER_PORT);
            appData.socket = TCPIP_TCP_ServerOpen(IP_ADDRESS_TYPE_IPV4, SERVER_PORT, 0);
            if (appData.socket == INVALID_SOCKET)
            {
                SYS_CONSOLE_MESSAGE("Couldn't open server socket\r\n");
                break;
            }
            appData.state = APP_TCPIP_WAIT_FOR_CONNECTION;
        }
        break;

        case APP_TCPIP_WAIT_FOR_CONNECTION:
        {
            if (!TCPIP_TCP_IsConnected(appData.socket))
            {
                return;
            }
            else
            {
                appData.state = APP_TCPIP_SERVING_CONNECTION;
                SYS_CONSOLE_MESSAGE("Received a connection\r\n");
            }
        }
        break;

        case APP_TCPIP_SERVING_CONNECTION:
        {
            if (!TCPIP_TCP_IsConnected(appData.socket) 
            	|| TCPIP_TCP_WasDisconnected(appData.socket))
            {
                appData.state = APP_TCPIP_CLOSING_CONNECTION;
                SYS_CONSOLE_MESSAGE("Connection was closed\r\n");
                break;
            }
            int16_t wMaxGet, wMaxPut, wCurrentChunk;
            uint16_t w, w2;
            uint8_t AppBuffer[32 + 1];
            
            // Figure out how many bytes have been received and how many we can transmit.
            wMaxGet = TCPIP_TCP_GetIsReady(appData.socket);	// Get TCP RX FIFO byte count
            wMaxPut = TCPIP_TCP_PutIsReady(appData.socket);	// Get TCP TX FIFO free space

            // Make sure we don't take more bytes out of the RX FIFO than we can
            // put into the TX FIFO
            if (wMaxPut < wMaxGet)
                    wMaxGet = wMaxPut;

            // Process all bytes that we can. This is implemented as a loop,
            // processing up to sizeof(AppBuffer) bytes at a time. This limits
            // memory usage while maximizing performance.  Single byte Gets and
            // Puts are a lot slower than multibyte GetArrays and PutArrays.
            wCurrentChunk = sizeof(AppBuffer) -1;
            for (w = 0; w < wMaxGet; w += sizeof(AppBuffer) - 1)
            {
				// Make sure the last chunk, which will likely be smaller than
				// sizeof(AppBuffer), is treated correctly.
				if (w + sizeof(AppBuffer) - 1 > wMaxGet) wCurrentChunk = wMaxGet - w;

                // Transfer the data out of the TCP RX FIFO and into our local
                // processing buffer.
                TCPIP_TCP_ArrayGet(appData.socket, AppBuffer, wCurrentChunk);

                // Perform the "ToUpper" operation on each data byte
                for (w2 = 0; w2 < wCurrentChunk; w2++)
                {
                    i = AppBuffer[w2];
                    if (i == '\x1b')   // escape
                    {
                        appData.state = APP_TCPIP_CLOSING_CONNECTION;
                        SYS_CONSOLE_MESSAGE("Connection was closed\r\n");
                    }
                }
                AppBuffer[w2] = 0;  // end the console string properly

                // Transfer the data out of our local processing buffer and into
                // the TCP TX FIFO.
                SYS_CONSOLE_PRINT("Server Sending %s\r\n", AppBuffer);
                TCPIP_TCP_ArrayPut(appData.socket, AppBuffer, wCurrentChunk);

                // No need to perform any flush. TCP data in TX FIFO will
                // automatically transmit itself after it accumulates for a
                // while. If you want to decrease latency at the expense of
                // wasting network bandwidth on TCP overhead, perform an
                // explicit flush via the TCPFlush() API.
            }
        }
        break;
        case APP_TCPIP_CLOSING_CONNECTION:
        {
            // Close the socket connection.
            TCPIP_TCP_Close(appData.socket);
            appData.socket = INVALID_SOCKET;
            appData.state = APP_TCPIP_WAIT_FOR_IP;
        }
        break;
        default:
        break;
    }
}


static void uart2_putc(char c)
{
    while (UART2_Write((uint8_t*)&c, 1) == 0); 
}

static void uart2_puts(const char *s)
{
    while (*s) uart2_putc(*s++);
}

static void uart2_puthex(uint32_t value)
{
    const char hex[] = "0123456789ABCDEF";
    for (int shift = 28; shift >= 0; shift -= 4)
        uart2_putc(hex[(value >> shift) & 0xF]);
}

/*
	uart2_send_int takes an integer value and transmits it, least significant
	byte first, over UART2. We can use it to broadcast raw register values, as
	in:

	uart2_send_int(RPF3R);
	uart2_send_int(LATF);
	uart2_send_int(PORTF);
	uart2_send_int(ODCF);
	uart2_send_int(RPF3R);

	Because the UART sends the least significant bit first, we will see the bits
	on our oscilloscope trace of UART2's TX line as bit 0 to 32, with a stop bit
	(1) and a start bit (0) between each of the bytes.	
*/
static void uart2_send_int(uint32_t value)
{
	int i;
	for (i = 0; i <= 3; i++) uart2_putc(((uint8_t*)&value)[i]);
}

/*
	SYS_Initialize performs a sequence of functions necessary when the CPU boots
	up. It calls routines defined in initialize.c and elsewhere in the
	configuration source code. The CLK routine intializes the clock. The MA
	routine initializes memory access: wait states and error code correction.
	The GPIO routine configures the MCU pins, including peripheral pin selection
	(PPS). The NVM routine initializes the non-volatile flash memory. The
	CORETIMER routine we have yet to investigate. The CONSOLE routine sets up
	UART2 as a console to transmit reporting. The TCPIP routine initializes the
	TCPIP stack.
*/
void SYS_Initialize (void* data)
{
	(void)__builtin_disable_interrupts();	
	CLK_Initialize();
	MA_Initialize();
	GPIO_Initialize();
	NVM_Initialize();
	CORETIMER_Initialize();
	CONSOLE_Initialize();
    TCPIP_Initialize();
	appData.state = APP_TCPIP_WAIT_INIT;
	(void)__builtin_enable_interrupts();
}

int main ( void )
{
	// Variables.
	int i;
	
	// Call the system initialization routine.
	SYS_Initialize(NULL);
	
	// Turn on the red and green lamps.
   	GPIO_PortSet(GPIO_PORT_A,0x00000004);
   	GPIO_PortSet(GPIO_PORT_C,0x00008000);
   	
   	// A while loop with a counter to control the state of our LEDs. It calls SYS_Tasks,
   	// which is supposed to maintain the TCP/IP server.
   	i=0;
    while (true) {
		/* Maintain system services */
		SYS_CMD_Tasks();
	
		/* Maintain Device Drivers */
		DRV_MIIM_OBJECT_BASE_Default.DRV_MIIM_Tasks(sysObj.drvMiim_0);
	
		/* Maintain Middleware & Other Libraries */
		TCPIP_STACK_Task(sysObj.tcpip);
	
		/* Maintain the application's state machine. */
		APP_Tasks();
		
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
    
    // The only reason to return is because of an error. We must pass back a value
    // of the correct type, as given by 
    return (EXIT_FAILURE);
}
