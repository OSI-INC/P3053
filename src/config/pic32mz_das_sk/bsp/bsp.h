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

// DOM-IGNORE-BEGIN
/*******************************************************************************
* Copyright (C) 2018 Microchip Technology Inc. and its subsidiaries.
*
* Subject to your compliance with these terms, you may use Microchip software
* and any derivatives exclusively with Microchip products. It is your
* responsibility to comply with third party license terms applicable to your
* use of third party software (including open source software) that may
* accompany Microchip software.
*
* THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES, WHETHER
* EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE, INCLUDING ANY IMPLIED
* WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS FOR A
* PARTICULAR PURPOSE.
*
* IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE,
* INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND
* WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP HAS
* BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE. TO THE
* FULLEST EXTENT ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL CLAIMS IN
* ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT OF FEES, IF ANY,
* THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS SOFTWARE.
*******************************************************************************/
// DOM-IGNORE-END

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
#define pic32mz_da_sk
#define BSP_NAME             "pic32mz_da_sk"


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

/*** SWITCH Macros for SWITCH2 ***/
#define SWITCH2_Get() ((PORTB >> 12) & 0x1U)
#define SWITCH2_STATE_PRESSED 0U
#define SWITCH2_STATE_RELEASED 1U

/*** SWITCH Macros for SWITCH3 ***/
#define SWITCH3_Get() ((PORTB >> 14) & 0x1U)
#define SWITCH3_STATE_PRESSED 0U
#define SWITCH3_STATE_RELEASED 1U

/*** SWITCH Macros for SWITCH1 ***/
#define SWITCH1_Get() ((PORTB >> 13) & 0x1U)
#define SWITCH1_STATE_PRESSED 0U
#define SWITCH1_STATE_RELEASED 1U




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

#endif // BSP_H

/*******************************************************************************
 End of File
*/
