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
#include "definitions.h"

static void uart2_putc(char c)
{
    while (UART2_Write((uint8_t*)&c, 1) == 0); 
}

/*
static void uart2_puts(const char *s)
{
    while (*s) uart2_putc(*s++);
}
*/

/*
static void uart2_puthex(uint32_t value)
{
    const char hex[] = "0123456789ABCDEF";
    for (int shift = 28; shift >= 0; shift -= 4)
        uart2_putc(hex[(value >> shift) & 0xF]);
}
*/

static void uart2_send_int(uint32_t value)
{
	int i;
	for (i = 0; i <= 3; i++) uart2_putc(((uint8_t*)&value)[i]);
}

int main ( void )
{
	// Variables.
	int i;
	
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
//			uart2_send_int(0x80800101);
			uart2_send_int(TRISF);
//			uart2_send_int(LATF);
//			uart2_send_int(PORTF);
//			uart2_send_int(ODCF);
//			uart2_send_int(RPF3R);
			GPIO_PortToggle(GPIO_PORT_F,0x00000008);
			GPIO_PortToggle(GPIO_PORT_A,0x00000004);
		}
		if (i==20000) i=0;
    }
    
    // The only reason to return is because of an error.
    return (EXIT_FAILURE);
}
