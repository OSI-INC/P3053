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

#include <stddef.h>                     // Defines NULL
#include <stdbool.h>                    // Defines true
#include <stdlib.h>                     // Defines EXIT_FAILURE
#include "definitions.h"                // SYS function prototypes

int main ( void )
{
	// Variables.
	int i;
	uint8_t msg[] = "ABC";
	
	// Call the system initialization routine, which is to be found in tasks.c and
	// with forward declaration in definitions.h.
	SYS_Initialize(NULL);
	
	// Set out two LEDs to be on or off as we like.
   	GPIO_PortSet(GPIO_PORT_F,0x00000008);
   	GPIO_PortClear(GPIO_PORT_A,0x00000004);
   	
   	// A while loop with a counter to control the state of our LEDs. It calls SYS_Tasks,
   	// which is supposed to maintain the TCP/IP server.
   	i=0;
    while (true) {
		SYS_Tasks();
		i = i+1;
		if (i==10000) {
			UART2_Write(msg, sizeof(msg) - 1);
			GPIO_PortToggle(GPIO_PORT_F,0x00000008);
			GPIO_PortToggle(GPIO_PORT_A,0x00000004);
		}
		if (i==20000) i=0;
    }
    
    // The only reason to return is because of an error.
    return (EXIT_FAILURE);
}
