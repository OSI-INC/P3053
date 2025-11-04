/*******************************************************************************
  Board Support Package Implementation

  Company:
    Microchip Technology Inc.

  File Name:
    bsp.c

  Summary:
    Board Support Package implementation.

  Description:
    This file contains routines that implement the board support package
*******************************************************************************/

#include "bsp.h"

// *****************************************************************************
/* Function:
    void BSP_Initialize(void)

  Summary:
    Performs the necessary actions to initialize a board

  Description:
    This function initializes the LED, Switch and other ports on the board.
    This function must be called by the user before using any APIs present in
    this BSP.

  Remarks:
    Refer to bsp.h for usage information.
*/


void BSP_Initialize(void)
{
    /* Configure LED pins as outputs */
    TRISFCLR = (1 << 3) | (1 << 2) | (1 << 8);  // RF3, RF2, RF8
    TRISACLR = (1 << 2);                        // RA2

    /* Switch off LEDs */
    LED1_On();  // Green
    LED2_On();  // Blue
    LED3_On();  // White
    LED4_On();  // Red
}