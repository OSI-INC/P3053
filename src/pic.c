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
#include "console.h"
#include "pic.h"

/*
	Define some macros for the configuration routines.
*/
#define CONFIG_MAX_LEN			512
#define CONFIG_FLASH_PAGE_ADDR  0x9D100000u
#define CONFIG_FLASH_ROW_ADDR   0x9D100000u

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

/*
	pic_configure sets the PIC32MZ general-purpose input-output pins for our
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
	
	// Lock the configuration registers. We write a value that is not one of
	// the three key values we use to unlock the registers.
	SYSKEY = 0x33333333;
	
    // Configure the LWDAQ Controller address bus lines as outputs.
    TRISCCLR = LWDAQ_CA_MASK_RC;   // RC1–RC4
    TRISECLR = LWDAQ_CA_MASK_RE;   // RE0–RE1

	// Configure the LWDAQ Controller data strobe and write lines as outputs.
	TRISDCLR = LWDAQ_DS_RD4 | LWDAQ_CW_RD5;

    // Set the LWDAQ Controller data bus initially as inputs.
    TRISCSET = LWDAQ_CD_MASK_RC;   // RC13, RC14
    TRISESET = LWDAQ_CD_MASK_RE;   // RE2–RE7

    // Unassert LWDAQ Controller data strobe and write.
    LATDSET = LWDAQ_DS_RD4 | LWDAQ_CW_RD5;

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
	
	// Initialize the system clock and phase locked loops.
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
	
	// Initialize the system timer module, which we need to time initialization
	// of the UART and TCP/IP stack.
	SYSTIME_Initialize();
	
	// Configure the functions of the gerneral-purpose input and output pins.
	pic_io_initialize();
	
	// Initialize the console, which communicates through a three-wire UART.
	console_initialize();
	
	// Set up the Ethernet physical interface and start up the TCP/IP stack.
	TCPIP_Initialize();
	
	// Re-enable interrupts and report initialization complete.
	(void)__builtin_enable_interrupts();
}

int config_save_string(const char* config) {
    uint8_t row[CONFIG_MAX_LEN];
    size_t len = strlen(config);

console_print("Before erase, first byte = %02X\r\n", 
              *(uint8_t*)CONFIG_FLASH_ROW_ADDR );

    if (len >= CONFIG_MAX_LEN) {len = CONFIG_MAX_LEN - 1;}
    memcpy(row, config, len);
    row[len] = '\0';
    for (size_t i = len + 1; i < CONFIG_MAX_LEN; i++) {
        row[i] = 0xFF;
    }
    if (!NVM_PageErase(CONFIG_FLASH_PAGE_ADDR)) {
        return -1;
    }
    while (NVM_IsBusy()) { ; }
    if (!NVM_RowWrite((uint32_t*)row, CONFIG_FLASH_ROW_ADDR)) {
        return -1;
    }
    while (NVM_IsBusy()) { ; }
console_print("After write, first byte = %02X\r\n", 
              *(uint8_t*)CONFIG_FLASH_ROW_ADDR );
	return 0;
}

int config_load_string(char* out, uint32_t out_size) {
    const uint8_t* row = (const uint8_t*)CONFIG_FLASH_ROW_ADDR;
    uint32_t i = 0;
    while ((i < CONFIG_MAX_LEN) && (row[i] != '\0')) {
    	i++;
    }
   	if (i == CONFIG_MAX_LEN) {
        return -1;
    }
    if (i + 1 > out_size) {
        return -1;
    }
    memcpy(out, row, i + 1);
	return 0;
}




