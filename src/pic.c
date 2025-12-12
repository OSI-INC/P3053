/*
	pic.c is a library of utility routines that communicate with the PIC32MZ
	registers, read and write throught he MPCIE parallel port, and read and
	write to the non-volatile memory (NVM).
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
	into the RowWrite routine.

	Most of the dynamic random-access memory (DRAM) available to the CPU is
	accessed by the CPU indirectly through a faster cache memory. Pages of DRAM
	are read into the cache and the CPU operates upon the cached copies. Every
	now and then, the microcontroller's memory management system writes the
	cached page to DRAM so that it can read a new page from DRAM into the cache
	for the CPU to use. When RowWrite copies from our RAM buffer to NVM, it uses
	the direct memory access (DMA) hardware in the microcontroller. The DMA
	hardware operates upon the DRAM, not upon the cache. If our buffer resides
	in the DRAM cache, we will write our row to the cache, and the DMA will
	operate upon the DRAM. Instead of seeing the row we just copied to our
	buffer appearing in NVM, we will see some row we copied previously appear in
	our NVM.

	In the PIC32MZ2048EFH, we have both cached and non-chached copies of the
	same 512 KByte of DRAM. The cached copy is at 0x80000000 to 0x8007FFFFF and
	the non-cached is at 0x80000000 to 0x8007FFFFF. The "coherent" declaration
	attribute tells the compiler to place a variable in the non-chached copy.
	Accesses to the non-cached copy will be direct, so we will write directly
	into the DRAM and the DMA will copy directly from DRAM into NVM.

	We also specify the "aligned" attribute with the value "32", which puts the
	buffer's first byte on a 32-byte boundary. We have done this not because we
	have observed ill-effects from failing to specify a 32-byte boundary, but
	because we want to avoid some pernicious bug in the future that arises from
	our buffer being mis-aligned with the boundaries expected by an NVM write
	routine. We have no evidence that failing to place the buffer on a 32-byte
	boundary will cause any problems, but we read passages in the code comments
	stating that there will be a problem if the buffer is not on a 4-byte
	boundary. So we're not taking any chances.

	We use our non-cached and aligned buffer in our pic_flash_write and
	pic_flash_read routines.
*/
static uint8_t pic_nvm_write_buff[NVM_FLASH_ROWSIZE]
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
	pic_nvm_writestr takes a pointer to a null-terminated string buffer and an
	address in non-volatile memory (NVM, or "flash memory") and copies the
	string from the buffer to flash memory. The buffer must reside in
	non-chached RAM. The NVM memory location can be either in the cashed or
	non-cached program flash address ranges. If we want to read the string back
	immediately and see what we just wrote, we should choose the non-cached
	address range. If we want to read repeatedly from the flash memory, we
	should choose the cached address range and somehow force a refresh of the
	page from flash after writing, although we have not yet figured out how to
	force such a refresh, other than to reset the microcontroller.
*/
int pic_nvm_writestr(const char* str, uint32_t flash_addr) {
    size_t len = strlen(str);
    uint32_t i = 0;
    if (len >= (NVM_FLASH_ROWSIZE - 1u)) {len = (NVM_FLASH_ROWSIZE - 1u);}
    memcpy(pic_nvm_write_buff, str, len);
    pic_nvm_write_buff[len] = '\0';
    for (i = len + 1; i < NVM_FLASH_ROWSIZE; i++) {pic_nvm_write_buff[i] = 0xFF;}
	if (!NVM_PageErase(flash_addr)) {return -1;}
	while (NVM_IsBusy()) {;}
    if (!NVM_RowWrite((uint32_t*)pic_nvm_write_buff, flash_addr)) {return -1;}
    while (NVM_IsBusy()) {;}
	return 0;
}

/*
	pic_nvm_readstr reads a null-terminated string from non-volatile memory
	(NVM) and copies the string into a buffer we provide. We pass as arguments
	the NVM address (flash memory address) we wish to read from, a pointer to
	the buffer we want to copy into, and the maximum size of the output buffer.
	If the flash address we specify is in the cached range of program flash, we
	will read from the cached copy of the NVM. If the address is in the
	non-cached range, we will read directly from NVM.
*/
int pic_nvm_readstr(uint32_t flash_addr, char* str, uint32_t str_size) {
	const uint8_t* row = (const uint8_t*)flash_addr;
	uint32_t i = 0;
	while ((i < NVM_FLASH_ROWSIZE) && (row[i] != '\0')) {i++;}
	if (i == NVM_FLASH_ROWSIZE) {return -1;}
	if (i + 1 > str_size) {return -1;}
	memcpy(str, row, i + 1);
	return 0;
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

/*
	pic_configure sets the PIC32MZ general-purpose input-output pins for our
	application.
*/
void pic_gpio_initialize(void) {
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
	pic_gpio_initialize();
	
	// Initialize the console, which communicates through a three-wire UART.
	console_initialize();
	
	// Set up the Ethernet physical interface and start up the TCP/IP stack.
	TCPIP_Initialize();
	
	// Re-enable interrupts and report initialization complete.
	(void)__builtin_enable_interrupts();
}

/*
	pic_config_write writes a null-terminated string to the configuration
	location in non-volatile memory.
*/
int pic_config_write(const char* config) {
	return pic_nvm_writestr(config, FLASH_CONFIG_ADDR);
}

/*
	pic_config_read reads a null-terminated string from the configuration
	location in non-volatile memory.
*/
int pic_config_read(char* config, uint32_t config_size) {
	int status;
	char scratch[255];
	status = pic_nvm_readstr(FLASH_CONFIG_ADDR, config, config_size);
	if (status < 0) {
		sprintf(config,"lwdaq_relay_configuration:\n");
		sprintf(scratch,"operator: unassigned\r\n");
		strcat(config,scratch);
		sprintf(scratch,"configuration_time: 00000000000000\r\n");
		strcat(config,scratch);
		sprintf(scratch,"password: LWDAQ\r\n");
		strcat(config,scratch);
		sprintf(scratch,"driver_id: unassigned\r\n");
		strcat(config,scratch);
		sprintf(scratch,"ip_addr: 10.0.0.37\r\n");
		strcat(config,scratch);
		sprintf(scratch,"ip_port: 90\r\n");
		strcat(config,scratch);
		sprintf(scratch,"tcp_timeout: 0\r\n");
		strcat(config,scratch);
		sprintf(scratch,"security_level: 0\r\n");
		strcat(config,scratch);
		sprintf(scratch,"gateway_addr: 10.0.0.1\r\n");
		strcat(config,scratch);
		sprintf(scratch,"subnet_mask: 255.255.255.0\r\n");
		strcat(config,scratch);		
	}
	return status;
}

/*
	uart2_readcount returns the number of bytes that are available in the UART2 
	input buffer. It takes as an argument a generic pointer, which allows the
	routine to be used in generic channel-descriptor records, such as those used
	by our command-line interpreter (CLI).
*/
int uart2_readcount(void *context)
{
    (void)context;
    return UART2_ReadCountGet();
}

/*
	uart2_getchar attempts to read the specified number of bytes from the UART2
	interface into a buffer. The routine will block until it receives all bytes
	it wants. We assume that the number of bytes requested has been set by the
	readcount routine, so the bytes are all waiting to be read. The context
	pointer is for use by channel descriptors.
*/
int uart2_getbytes(void *context, uint8_t* buff, uint32_t len) {
    (void) context;
    int num_got = (int) UART2_Read(buff, len);
    return num_got;
}

/*
	uart2_getchar attempts to read a single character from the UART2 interface.
	If it succeeds, it returns the byte value in an integer. If it fails, it
	returns a -1. The context pointer is for use by channel descriptors.
*/
int uart2_getchar(void *context) {
    (void) context;
    uint8_t c;
    if (UART2_Read(&c, 1) == 1) return c; 
    return -1;
}

/*
	uart2_putbytes attempts to write a specified number of bytes to the UART2
	interface. It waits until all bytes have been written. It never returns an
	error. We trust it will not block or hang. The context pointer is for use by
	channel descriptors. One detail in the code: Harmony's UART2 write routine
	expects a buffer that is declared modifiable, even though it does not write
	to the buffer. So we must type-cast the buffer into a modifiable buffer to
	avoid a compiler warning.
*/
int uart2_putbytes(void *context, const uint8_t* buff, uint32_t len) {
    (void)context;
    for (uint32_t i = 0; i < len; i++) {
        while (UART2_Write((uint8_t*) &buff[i], 1) == 0);
    }
    return (int) len;
}

/*
	uart2_putchar writes a character to the UART2 interface. It blocks until the
	character is written. It returns a 1 to indicate that one byte was written. 
	The context pointer is for use by channel descriptors.
*/
int uart2_putchar(void *context, char c) {
    (void) context;
	while (UART2_Write((uint8_t*)&c, 1) == 0);
	return 1;
}

/*
	uart2_flush is a CLI-ready communication routine. It waits until all bytes
	waiting to be transmitted by the UART2 have been transmitted, and then
	returns. It blocks until transmission is complete. It always returns a zero
	to indicate success. The context pointer is for use by channel descriptors.
*/
int uart2_flush(void *context) {
    (void) context;
    while (UART2_WriteCountGet() != 0) { ; }
    while (U2STAbits.UTXBF == 1) { ; }
    while (U2STAbits.TRMT == 0) { ; }
    return 0;
}
