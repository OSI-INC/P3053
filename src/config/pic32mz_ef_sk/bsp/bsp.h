/*******************************************************************************
  Board Support Package Header File.

  Company:
    Microchip Technology Inc.

  File Name:
    bsp.h

  Summary:
    Board Support Package Header File 

  Description:
    This file contains constants, macros, type definitions and function
    declarations 
*******************************************************************************/

#ifndef BSP_H
#define BSP_H

// *****************************************************************************
// *****************************************************************************
// Section: Included Files
// *****************************************************************************
// *****************************************************************************

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "device.h"

// *****************************************************************************
// *****************************************************************************
// Section: BSP Macros
// *****************************************************************************
// *****************************************************************************
#define pic32mz_ef_sk
#define BSP_NAME		"pic32mz_ef_sk"

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


// *****************************************************************************
// *****************************************************************************
// Section: Interface Routines
// *****************************************************************************
// *****************************************************************************

// *****************************************************************************
/* Function:
    void BSP_Initialize(void)

  Summary:
    Performs the necessary actions to initialize a board

  Description:
    This function initializes the LED and Switch ports on the board.  This
    function must be called by the user before using any APIs present on this
    BSP.

  Precondition:
    None.

  Parameters:
    None

  Returns:
    None.

  Example:
    <code>
    BSP_Initialize();
    </code>

  Remarks:
    None
*/

void BSP_Initialize(void);

#endif 