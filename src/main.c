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

/*** LED Macros for Green LED on RF3 ***/
#define LED1_Toggle()   (LATFINV = (1UL<<3))
#define LED1_Get()      ((PORTF >> 3) & 0x1U)
#define LED1_On()       (LATFSET = (1UL<<3))
#define LED1_Off()      (LATFCLR = (1UL<<3))

/*** LED Macros for Blue LED on RF2 ***/
#define LED2_Toggle()   (LATFINV = (1UL<<2))
#define LED2_Get()      ((PORTF >> 2) & 0x1U)
#define LED2_On()       (LATFSET = (1UL<<2))
#define LED2_Off()      (LATFCLR = (1UL<<2))

/*** LED Macros for White LED on RF8 ***/
#define LED3_Toggle()   (LATFINV = (1UL<<8))
#define LED3_Get()      ((PORTF >> 8) & 0x1U)
#define LED3_On()       (LATFSET = (1UL<<8))
#define LED3_Off()      (LATFCLR = (1UL<<8))

/*** LED Macros for Red LED on RA2 ***/
#define LED4_Toggle()   (LATAINV = (1UL<<2))
#define LED4_Get()      ((PORTA >> 2) & 0x1U)
#define LED4_On()       (LATASET = (1UL<<2))
#define LED4_Off()      (LATACLR = (1UL<<2))


int main ( void )
{
	TRISFCLR = (1 << 3) | (1 << 2) | (1 << 8);  // RF3, RF2, RF8
	TRISACLR = (1 << 2);                        // RA2
	LED1_On();
	LED2_On();
	LED3_On();
	LED4_On();

    SYS_Initialize ( NULL );

    while ( true )
    {
        SYS_Tasks ( );
    }

    return ( EXIT_FAILURE );
}
