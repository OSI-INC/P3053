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
	The buffer we use when writing to non-volatile memory (NVM). We are reading
	plib_nvm.h through definitions.h. The NVM header file declares the constants
	that define the address range, erase page size and write row size of the
	program flash memory, as well as the routines we use to erase and write to
	the NVM, these being NVM_PageErase, NVM_IsBusy, and NVM_RowWrite. In order
	to write to NVM, we need a row-sized buffer in RAM that we can fill with the
	content we wish to write, and then we will pass a pointer to this buffer
	into the RowWrite routine. Most of the dynamic random-access memory (DRAM)
	available to the CPU is accessed by the CPU indirectly through a faster
	cache memory. Pages of DRAM are read into the cache and the CPU operates
	upon the cached copies. Every now and then, the microcontroller's memory
	management system writes the cached page to DRAM so that it can read a new
	page from DRAM into the cache for the CPU to use. When RowWrite copies from
	our RAM buffer to NVM, it uses the direct memory access (DMA) hardware in
	the microcontroller. The DMA hardware operates upon the DRAM, not upon the
	cache. If our buffer resides in the DRAM cache, we will write our row to the
	cache, and the DMA will operate upon the DRAM. Instead of seeing the row we
	just copied to our buffer appearing in NVM, we will see some row we copied
	previously appear in our NVM. In the PIC32MZ2048EFH, we have both cached and
	non-chached copies of the same 512 KByte of DRAM. The cached copy is at
	0x80000000 to 0x8007FFFFF and the non-cached is at 0x80000000 to
	0x8007FFFFF. The "coherent" declaration attribute tells the compiler to
	place a variable in the non-chached copy. Accesses to the non-cached copy
	will be direct, so we will write directly into the DRAM and the DMA will
	copy directly from DRAM into NVM. We also specify the "aligned" attribute
	with the value "32", which puts the buffer's first byte on a 32-byte
	boundary. We have done this not because we have observed ill-effects from
	failing to specify a 32-byte boundary, but because we want to avoid some
	pernicious bug in the future that arises from our buffer being mis-aligned
	with the boundaries expected by an NVM write routine. We have no evidence
	that failing to place the buffer on a 32-byte boundary will cause any
	problems, but we read passages in the code comments stating that there will
	be a problem if the buffer is not on a 4-byte boundary. So we're not taking
	any chances.
*/
static uint8_t flash_write_buff[NVM_FLASH_ROWSIZE]
    __attribute__((coherent, aligned(32)));

/*
	The location of our configuration string in non-volatile memory (NVM). The
	PIC32MZ2048EFH's 2 MByte of NVM appears twice in the CPU's virtual address
	space. Once in the range 0x9D000000 to 0x9D0FFFFF, in which range the NVM is
	accessed by the CPU indirectly through a cache memory, and again in the
	range 0xBD000000 to 0xBD0FFFFF, in which range the CPU accesses the NVM
	directly, without a cache. We want to put our string in the un-cached copy
	so that we can write with the direct memory access (DMA) hardware and read
	back immediately from the physical NVM rather than getting a stale copy of
	the NVM from cache. So we place our string in the 0xBD range. In that range,
	we must make sure we are above the program itself. In our case, our program
	is less than 256 KByte, so anywhere above 0xBD040000 will be fine. The NVM
	pages are 16 Kbyte in the PIC32MZ, and NVM rows are 2 KByte. So we must pick
	a location that is on a 16-KByte boundary. Reading repeatedly from un-cached
	NVM is far slower than reading repeatedly from a cached copy of an NVM page,
	but we do not plan to read repeatedly from our configuration string, so we
	will suffer no loss of performance from reading direction from NVM.
*/
#define FLASH_CONFIG_ADDR	0xBD100000

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
    size_t len = strlen(config);
    uint32_t i = 0;
    if (len >= (NVM_FLASH_ROWSIZE - 1u)) {len = (NVM_FLASH_ROWSIZE - 1u);}
    memcpy(flash_write_buff, config, len);
    flash_write_buff[len] = '\0';
    for (i = len + 1; i < NVM_FLASH_ROWSIZE; i++) {flash_write_buff[i] = 0xFF;}
	if (!NVM_PageErase(FLASH_CONFIG_ADDR)) {return -1;}
	while (NVM_IsBusy()) {;}
    if (!NVM_RowWrite((uint32_t*)flash_write_buff, FLASH_CONFIG_ADDR)) { return -1; }
    while (NVM_IsBusy()) {;}
	return 0;
}

int config_load_string(char* out, uint32_t out_size) {
	const uint8_t* row = (const uint8_t*)FLASH_CONFIG_ADDR;
	uint32_t i = 0;
	while ((i < NVM_FLASH_ROWSIZE) && (row[i] != '\0')) { i++; }
	if (i == NVM_FLASH_ROWSIZE) { return -1; }
	if (i + 1 > out_size) { return -1; }
	memcpy(out, row, i + 1);
	return 0;
}




