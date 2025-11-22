/*
	utils.c is a library of utility routines that communicate with the PIC32MZ
	registers, read and write throught he MPCIE parallel port, and perform
	various mundane tasks with strings and buffers.
*/

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>                  
#include <stdbool.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#include "configuration.h"
#include "definitions.h"
#include "pic.h"
#include "console.h"
#include "server.h"

/*
	pic_io_initialize configures the general-purpose input-output pins for our
	application.
*/
void pic_io_initialize(void) {
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
	pic_initialize sets up the embedded microcontroller and its Ethernet interface. It
	starts up all system modules to support TCP/IP servers, interrupt timers, flash
	memory use, and the UART console.
*/
void pic_initialize(void) {

	// Disable interrupts for initialization.
	(void)__builtin_disable_interrupts();	
	
	// Initialize the system clock and phase locked loops. This must come first.
	CLK_Initialize();
	
	// Enable prefetch for both instructions and data.
	PRECONbits.PREFEN = 3;
	
	// Set the number of program flash memory wait states.
	PRECONbits.PFMWS = 3;
	
	// Configure the flash memory error correction to force correction of
	// single-bit errors and report double-bit errors.
	CFGCONbits.ECCCON = 3;
	
	// Initialize the non-volatile memory controller, clear NVM error flags.
	// Necessary if we use the flash memory.
	NVM_Initialize();
	
	// Initialize the hardware core timer routines. These allows us to implement
	// interrupt-driven timers and delays.
	CORETIMER_Initialize();
	
	// Configure the functions of the gerneral-purpose input and output pins.
	pic_io_initialize();
	
	// Initialize the console, which communicates through a three-wire UART.
	console_initialize();
	
	// Set up the Ethernet physical interface and start up the TCP/IP stack.
//	tcpip_initialize();
	
	// Re-enable interrupts and report initialization complete.
	(void)__builtin_enable_interrupts();
}

/*
	pic_reset resets the Embedded Etherent Module. It does so by unlocking the
	PIC32MZ configuration registers and writing to the reset configuration bit.
	We make three writes to the thirty-two bit SYSKEY register with the correct
	combination to unlock the configuration bits. Then we set the RSWRSTSET
	register equal to a value defined in the device pack: "_RSWRST_SWRST_MASK".
	We read the reset register after that, which completes the initiation of
	reset. The routine does not return, it puts itself in an infinite loop,
	trusting that the reset will take place and reboot the system.
*/
void pic_reset(void) {
    SYSKEY = 0x00000000;
    SYSKEY = 0xAA996655;
    SYSKEY = 0x556699AA;
    RSWRSTSET = _RSWRST_SWRST_MASK;
    (void) RSWRST;
    while(1);
}



