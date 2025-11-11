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
#include <stddef.h>                  
#include <stdbool.h>
#include <stdlib.h>
#include "configuration.h"
#include "definitions.h"


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


static void uart2_send_int(uint32_t value)
{
	int i;
	for (i = 0; i <= 3; i++) uart2_putc(((uint8_t*)&value)[i]);
}

/*
	SYS_Initialize performs a series of initialization functions necessary when
	the CPU boots up. It calls routines defined in initialize.c and elsewhere in
	the configuration source code. The CLK routine intializes the clock. The MA
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
	(void)__builtin_enable_interrupts();
}

int main ( void )
{
	// Variables.
	int i;
	
	// Call the system initialization routine.
	SYS_Initialize(NULL);
	
	// Set out two LEDs to be on or off as we like.
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
		if (i==200000) {
			GPIO_PortToggle(GPIO_PORT_A,0x00000004);
//			uart2_send_int(RPF3R);
//			uart2_send_int(LATF);
//			uart2_send_int(PORTF);
//			uart2_send_int(ODCF);
//			uart2_send_int(RPF3R);
			i=0;
		}
    }
    
    // The only reason to return is because of an error.
    return (EXIT_FAILURE);
}
