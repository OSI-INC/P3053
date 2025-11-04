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

int main(void)
{
    // Init RED LED (RA2)
    TRISACLR = (1 << 2);
    LATACLR = (1 << 2);  // RED OFF

    // Init GREEN LED (RF3)
    TRISFCLR = (1 << 3);
    LATFSET = (1 << 3);  // Try turning it ON

    while (1)
    {
        // toggle every half second
        LATFINV = (1 << 3);
        LATAINV = (1 << 2);
        for (volatile int i = 0; i < 1000000; ++i);
    }
}
