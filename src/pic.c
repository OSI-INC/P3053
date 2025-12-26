/*
	pic.c -- Implementation of the PIC32MZ Utility library for the Embedded
	Ethernet Module.

	(C) 2025, Kevan Hashemi, Open Source Instruments Inc.

	This program is free software: you can redistribute it and/or modify it
	under the terms of the GNU General Public License as published by the Free
	Software Foundation, either version 3 of the License, or (at your option)
	any later version.

	This program is distributed in the hope that it will be useful, but WITHOUT
	ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
	FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
	more details.

	You should have received a copy of the GNU General Public License along with
	this program.  If not, see <https://www.gnu.org/licenses/>.
*/

/*
	The standard C headers we trust the compiler will know how to find. The
	Microchip library headers are configuration.h and definitions.h. The
	compiler must be told where to look for these two files. The remaining
	interfaces are those that go with our EEM implementation files.
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
#include "utils.h"
#include "config.h"
#include "console.h"
#include "server.h"
#include "cli.h"
#include "pic.h"

/*
	The buffer we use when writing to flash memory. We are reading plib_nvm.h
	through definitions.h. The flash header file declares the constants that
	define the address range, erase page size and write row size of the program
	flash memory, as well as the routines we use to erase and write to the
	flash, these being NVM_PageErase, NVM_IsBusy, and NVM_RowWrite (here "NVM"
	is for "non-volatile memory", which in the PIC32MZ is flash memory). In
	order to write to flash, we need a row-sized buffer in RAM that we can fill
	with the content we wish to write, and then we will pass a pointer to this
	buffer into the RowWrite routine.

	Most of the dynamic random-access memory (DRAM) available to the CPU is
	accessed by the CPU indirectly through a faster cache memory. Pages of DRAM
	are read into the cache and the CPU operates upon the cached copies. Every
	now and then, the microcontroller's memory management system writes the
	cached page to DRAM so that it can read a new page from DRAM into the cache
	for the CPU to use. When RowWrite copies from our RAM buffer to flash, it
	uses the direct memory access (DMA) hardware in the microcontroller. The DMA
	hardware operates upon the DRAM, not upon the cache. If our buffer resides
	in the DRAM cache, we will write our row to the cache, and the DMA will
	operate upon the DRAM. Instead of seeing the row we just copied to our
	buffer appearing in flash, we will see some row we copied previously appear
	in our flash.

	In the PIC32MZ2048EFH, we have both cached and non-chached copies of the
	same 512 KByte of DRAM. The cached copy is at 0x80000000 to 0x8007FFFFF and
	the non-cached is at 0x80000000 to 0x8007FFFFF. The "coherent" declaration
	attribute tells the compiler to place a variable in the non-chached copy.
	Accesses to the non-cached copy will be direct, so we will write directly
	into the DRAM and the DMA will copy directly from DRAM into flash.

	We also specify the "aligned" attribute with the value "32", which puts the
	buffer's first byte on a 32-byte boundary. We have done this not because we
	have observed ill-effects from failing to specify a 32-byte boundary, but
	because we want to avoid some pernicious bug in the future that arises from
	our buffer being mis-aligned with the boundaries expected by an flash write
	routine. We have no evidence that failing to place the buffer on a 32-byte
	boundary will cause any problems, but we read passages in the code comments
	stating that there will be a problem if the buffer is not on a 4-byte
	boundary. So we're not taking any chances.

	We use our non-cached and aligned buffer in our pic_flash_write and
	pic_flash_read routines.
*/
static uint8_t pic_flash_write_buff[NVM_FLASH_ROWSIZE]
    __attribute__((coherent, aligned(32)));

/*
	pic_flash_putbytes writes an array of bytes to flash memory. It takes as
	arguments an address in flash memory, a pointer to a byte array, and a
	number of bytes to write. When we copy from RAM to flash, the buffer from
	which we copy must reside in non-chached RAM because the NVM_RowWrite
	routine uses direct memory access (DMA) to make the transfer, and the DMA
	engine uses physical addresses of actual memory locations, not cached copies
	of memory. We first copy the bytes into our non-cached flash memory write
	buffer, then from that buffer into flash memory. The flash memmory
	destination location can be either in the cashed or non-cached flash memmory
	address ranges. If we want to read the bytes back immediately and see what
	we just wrote, we should choose the non-cached address range. If we want to
	read repeatedly from the flash memory, we should choose the cached address
	range and somehow force a refresh of the page from flash after writing. We
	have not yet figured out how to force such a refresh, other than to reset
	the microcontroller.

	The routine erases an entire page of memory and then writes not more than
	one row to the flash memmory. On the PIC32MZ, the pages are 16 KB and the
	rows are 2 KB. The destination address in flash memmory should lie on a
	16-KB boundary or we may have trouble with the erase procedure. The routine
	is wasteful of flash memmory space in that it makes no use of the other 14
	KB in the page, nor does it permit any other process to use the other 14 KB.
	If the number of specified length of the copy is greater than a row, we
	truncate it to one row. If the length is less than one row, we fill the rest
	of the row with erase bytes 0xFF. The routine returns the number of bytes it
	wrote, not counting erase bytes.
*/
int pic_flash_putbytes(uint32_t flash_addr, const uint8_t* buff, uint32_t len) {
    uint32_t i = 0;
    if (len >= NVM_FLASH_ROWSIZE) {len = NVM_FLASH_ROWSIZE;}
    memcpy(pic_flash_write_buff, buff, len);
    for (i = len; i < NVM_FLASH_ROWSIZE; i++) {pic_flash_write_buff[i] = 0xFF;}
	if (!NVM_PageErase(flash_addr)) {return -1;}
	while (NVM_IsBusy()) {;}
    if (!NVM_RowWrite((uint32_t*) pic_flash_write_buff, flash_addr)) {return -1;}
    while (NVM_IsBusy()) {;}
	return len;
}

/*
	pic_flash_writestr copies a string to flash memory. We pass it an address in
	flash memmory and a pointer to a null-terminated string. The routine calls
	pic_flash_putbytes to perform the copy. It returns the number of characters
	written, which will be the string length plus one, or an error code.
*/
int pic_flash_writestr(uint32_t flash_addr, const char* str) {
    uint32_t len = strlen(str) + 1;
    return pic_flash_putbytes(flash_addr, (const uint8_t*) str, len);
}

/*
	pic_flash_getbytes reads an array of bytes from flash memory into a byte
	array in RAM. We pass as arguments the byte array address we want to write
	to, the flash memmory address we want to read from, and the number of bytes
	to copy. If the flash address we specify is in the cached range of program
	flash, we will read from the cached copy of the flash memmory. If the
	address is in the non-cached range, we will read directly from flash
	memmory. The routine returns the number of bytes it read. Reading from flash
	memmory is not subject to the same 2-KB boundary constraint as writing to
	flash memmory. We can read from any address and we can read any number of
	bytes, by simply using memcpy.
*/
int pic_flash_getbytes(uint8_t* buff, uint32_t flash_addr, uint32_t len)  {
	const uint8_t* row = (const uint8_t*) flash_addr;
	memcpy(buff, row, len);
	return len;
}

/*
	pic_flash_readstr reads a null-terminated string from flash memory and
	copies the string into a buffer we provide. We pass as arguments the flash
	memory we wish to read from, a pointer to the buffer we want to copy into,
	and the maximum length of string we can copy. The flash address does not
	need to be on a 2-KB boundary. The routine scans forward from the flash
	address looking for a null, then copies all characters up to and including
	the null to the destination string. It returns the length of the string it
	copied. If it does not find a null, it will make an empty string and return
	a negative value.
*/
int pic_flash_readstr(char* str, uint32_t flash_addr, uint32_t str_size) {
	const uint8_t* row = (const uint8_t*) flash_addr;
	uint32_t i = 0;
	while ((i < str_size) && (row[i] != '\0')) {i++;}
	if (i + 1 > str_size) {
		str[0] = '\0';
		return -1;
	}
	memcpy(str, row, i + 1);
	return i;
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
	
    // Configure the MPCIE address bus lines as outputs.
    TRISCCLR = MPCIE_CA_MASK_RC;   // RC1–RC4
    TRISECLR = MPCIE_CA_MASK_RE;   // RE0–RE1

	// Configure the MPCIE data strobe and write lines as outputs.
	TRISDCLR = MPCIE_DS_RD4 | MPCIE_CW_RD5;

    // Set the MPCIE data bus initially as inputs.
    TRISCSET = MPCIE_CD_MASK_RC;   // RC13, RC14
    TRISESET = MPCIE_CD_MASK_RE;   // RE2–RE7

    // Unassert MPCIE bus data strobe and control write.
    LATDSET = MPCIE_DS_RD4 | MPCIE_CW_RD5;

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
	
	// Initialize the flash memory controller, clear flash memory error flags.
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
	pic_info prints a list of microcontroller information, such as timer and
	clock frequencies, and anything else we think is interesting or possibly
	helpful. The routine simply writes to a buffer. The calling process must
	ensure that the buffer is big enough. We do not bother implementing any
	check in this routine of the buffer size.
*/
void pic_info(char* out) {
	sprintf(out, 
		"Tick Counter %.3f (kHz)\n"
		"Sys Counter  %.3f (MHz)\n"
		"Sys Clock    %.3f (MHz)",
		(double) SYS_TMR_TickCounterFrequencyGet() * 1e-3,
		(double) SYS_TMR_SystemCountFrequencyGet() * 1e-6,
		(double) SYS_TMR_SystemCountFrequencyGet() * 2e-6);
	return;
}

/*
	uart2_readcount returns the number of bytes that are available in the UART2
	input buffer. It takes as an argument a generic pointer, which allows the
	routine to be used in generic channel-descriptor records, such as those used
	by our Command-Line Interpreter (CLI). Our UART2 routines make no use of
	this pointer, so they mark it as "void" to stop a compiler warning about an
	unused variable.
*/
int uart2_readcount(void *context)
{
    (void) context;
    return UART2_ReadCountGet();
}

/*
	uart2_read attempts to read the specified number of bytes from the UART2
	interface into a buffer. The routine will block until it receives all bytes
	it wants. We assume that the number of bytes requested has been set by the
	readcount routine, so the bytes are all waiting to be read. The context
	pointer is for use by channel descriptors.
*/
int uart2_read(void *context, uint8_t* buff, uint32_t len) {
    (void) context;
    int num_got = (int) UART2_Read(buff, len);
    return num_got;
}

/*
	uart2_getchar attempts to read a single character from the UART2 interface.
	If it succeeds, it returns the byte value in an integer. If it fails, it
	returns a -1. The context pointer is for use by channel descriptors. The
	routine never returns a -2, which is the code our CLI expects for a channel
	closure, because the UART will never close.
*/
int uart2_getchar(void *context) {
    (void) context;
    uint8_t c;
    if (UART2_Read(&c, 1) == 1) return (int) c; 
    return -1;
}

/*
	uart2_write attempts to write a specified number of bytes to the UART2
	interface. It waits until all bytes have been written. It never returns an
	error. We trust it will not block or hang. The context pointer is for use by
	channel descriptors. One detail in the code: Harmony's UART2 write routine
	expects a buffer that is declared modifiable, even though it does not write
	to the buffer. So we must type-cast the buffer into a modifiable buffer to
	avoid a compiler warning.
*/
int uart2_write(void *context, const uint8_t* buff, uint32_t len) {
    (void)context;
    for (uint32_t i = 0; i < len; i++) {
        while (UART2_Write((uint8_t*) &buff[i], 1) == 0);
    }
    return (int) len;
}

/*
	uart2_putchar writes a character to the UART2 interface. It blocks until the
	character is written. It returns a 1 to indicate that one byte was written.
	It blocks until the write is complete. It never returns an error. The
	context pointer is for use by channel descriptors.
*/
int uart2_putchar(void *context, char c) {
    (void) context;
	while (UART2_Write((uint8_t*)&c, 1) == 0);
	return 1;
}

/*
	uart2_flush waits until all bytes waiting to be transmitted by the UART2
	have been transmitted, and then returns. It blocks until transmission is
	complete. It always returns a zero to indicate success. The context pointer
	is for use by channel descriptors.
*/
int uart2_flush(void *context) {
    (void) context;
    while (UART2_WriteCountGet() != 0) { ; }
    while (U2STAbits.UTXBF == 1) { ; }
    while (U2STAbits.TRMT == 0) { ; }
    return 0;
}

/*
	cli_reset is a Command-Line Interpreter (CLI) procedure that performs a
	software reset of the Embedded Ethernet Module (EEM). It notifies the client
	on the CLI that the reset is about to take place, waits for a fraction of a
	second to make sure this announcement propagates, and resets the PIC32MZ.
*/
void cli_reset(cli_chan_type *ch, char *args) {
	bool print_info = false;
	bool print_help = false;
	char* tok = strtok(args, " \t");

	while (tok != NULL) {
 		if (strcmp(tok, "--info") == 0) {print_info = true;} 
 		else if (strcmp(tok, "--help") == 0) {print_help = true;} 
 		else {
            cli_print(ch, "ERROR: Unrecognized option '%s' in %s.\n", tok, __func__);
            return;
        }
        tok = strtok(NULL, " \t");
    }
    
	if (print_info) {
		cli_message(ch, "Initiate a software reset of the EEM.\n");
		return;
	}

	if (print_help) {
		cli_message(ch,
"Usage:\n"
"  reset [--info] [--help]\n"
"\n"
"Summary:\n"
"  Performs a software reset of the Embedded Ethernet Module (EEM). With no options,\n"
"  the command prints a reset notification. It waits for a quarter-second before\n"
"  initiating the reset.\n"
"\n"
"Options:\n"
"  --info        Print a one-line summary of this command.\n"
"  --help        Print this help text.\n"
		);
		return;
	}
	
	cli_message(ch, "Resetting the EEM...\n");
	int counter = 1e6;
	while (counter > 0) {
		tcp_tick();
		counter--;
	}
	pic_reset();
    return;
}

/*
	cli_mpcie is a Command-Line Interpreter (CLI) procedure that reads and
	writes form the MPCIE bus. We can install this procedure in the CLI and so
	gain access to the MPCIE bus with byte-wize reads and writes.
*/
void cli_mpcie(cli_chan_type *ch, char *args) {
	bool print_info = false;
	bool print_help = false;
	bool decimal_format = false;
	bool packed_format = false;
	bool write_access = false;
	bool verb_received = false;
	bool stream_access = false;
	uint8_t addr = 0;
	uint8_t val = 0;
	uint8_t data[CLI_MPCIE_MAX_DATA];
	uint32_t data_len = 0;
	uint32_t read_len = 1;
	
	
	char* tok = strtok(args, " \t");
	while (tok != NULL) {
 		if (strcmp(tok, "--info") == 0) {
 			print_info = true;
 			break;
 		} 
 		
 		if (strcmp(tok, "--help") == 0) {
 			print_help = true;
 			break;
 		} 
 		
 		if (strcmp(tok, "read") == 0 || strcmp(tok, "stream-read") == 0
 				  || strcmp(tok, "write") == 0 || strcmp(tok, "stream-write") == 0) {
 			verb_received = true;
 			if (strcmp(tok, "write") == 0 || strcmp(tok, "stream-write") == 0) {
 				write_access = true;
 			}
 			if (strcmp(tok, "stream-read") == 0 || strcmp(tok, "stream-write") == 0) {
 				stream_access = true;
 			}
 			tok = strtok(NULL, " \t");
 			if (!parse_uint8(tok, &addr)) {
 				cli_print(ch, "ERROR: Invalid address '%s' in %s.\r\n", tok, __func__);
				return;
			} 
        	tok = strtok(NULL, " \t");
			continue;
 		} 
 		
		if (!verb_received) {
			cli_print(ch, "ERROR: Must specify bus access type in %s.\n", __func__);
			return;
		}
		
		if (strcmp(tok, "--packed") == 0) {
 			packed_format = true;
 		} else if (strcmp(tok, "--decimal") == 0) {
 			decimal_format = true;
 		} else if (parse_uint32(tok, &read_len)) {
 			if (write_access && parse_uint8(tok, &val)) {
 				data[data_len] = val;
 				data_len++;
 				if (data_len > CLI_MPCIE_MAX_DATA) {
 					cli_print(ch, "ERROR: Attempt to write >%u bytes in %s.\n",
 						CLI_MPCIE_MAX_DATA, __func__);
            		return;
 				}
 			} else if (write_access && !parse_uint8(tok, &val)) {
				cli_print(ch, "ERROR: Address '%u' out of range in %s.\n", 
					val, __func__);
				return;
			} else if (read_len > CLI_MPCIE_MAX_DATA) {
				cli_print(ch, "ERROR: Attempt to read >%u bytes in %s.\n",
					CLI_MPCIE_MAX_DATA, __func__);
				return;
 			} else {
 				;
 			}
 		} else {
            cli_print(ch, "ERROR: Unrecognized option '%s' in %s.\n", tok, __func__);
            return;
        }
        
        tok = strtok(NULL, " \t");
    }
    
	if (print_info) {
		cli_message(ch, "Read and write from the eight-bit mPCIe bus.\n");
		return;
	}

	if (print_help) {
		cli_message(ch,
"Usage:\n"
"  mpcie <read|stream-read> <addr> [length] [--decimal|--packed] [--info] [--help]\n"
"  mpcie <write|stream-write> <addr> [--info] [--help] <value> [value...]\n"
"  \n"
"Summary:\n"
"  Writes to and reads from the mPCIe eight-bit bus. For both actions, we must\n"
"  specify an eight-bit address 'addr' either as hexadecimal with a '0x' prefix or\n"
"  as decimal with no prefix. The 'write' and 'stream-write' operations write as\n"
"  many values as we pass on the command line. The 'write' operation starts writing\n"
"  at location 'addr' and increments the location after each write. The\n"
"  'stream-write' writes all bytes to the same locatin 'addr'. The command reads\n"
"  bytes as a space-delimited string either in hexadecimal or decimal, just as for\n"
"  the 'addr'. The 'read' operation reads 'length' bytes from consecutive\n"
"  locations, starting at location 'addr'. The 'stream-read' operation reads\n"
"  'length' bytes from the same location 'addr'. If we omit 'length', it defaults\n"
"  to 1. By default, the returned bytes will be written as sixteen bytes to a line,\n"
"  each byte two hexadecimal digits separated from its neighbors by a space, and\n"
"  with each line of sixteen bytes starting with the mPCIe address of the first\n"
"  byte, expressed in hexadecimal with a '0x' prefix. With the '--decimal' option,\n"
"  the bytes read will be expressed as decimal values separated by spaces. With the\n"
"  'compact' option, the bytes will be returned as a packed string of hexadecimal\n"
"  bytes, each byte two digits only, with no separators, no newlines, and no\n"
"  address markers.\n"
"  \n"
"Verbs:\n"
"  read         Read multiple bytes from consecutive mPCIe locations.\n"
"  stream-read  Read multiple bytes from the same mPCIe location.\n"
"  write        Write multiple bytes to consecutive mPCIe locations.\n"
"  stream-write Write multiple bytes to the same mPCIe location.\n"
"  \n"
"Selectors:\n"
"  length       The number of bytes to read.\n"
"  \n"
"Options:\n"
"  --decimal    Display bytes read as positive decimal values.\n"
"  --packed     Display bytes read as packed hexadecimal.\n"
"  --info       Display a one-line summary of command, no bus access.\n"
"  --help       Display this help text, no bus access.\n"
		);
		return;
	}
	
	if (!verb_received) {
		cli_print(ch, "ERROR: No verb or option specified in %s.\n", __func__);
		return;
	}

	if (packed_format && decimal_format) {
		cli_print(ch, 
			"ERROR: Options '--packed' and '--decimal' incompatible in %s.\n", __func__);
		return;
	}

	if (!write_access) {
		if (!packed_format) {
			while (read_len > 0) {
				uint32_t line_len = (read_len > 16) ? 16 : read_len;
				cli_print(ch, "0x%02X: ", addr);
				for (uint32_t i = 0; i < line_len; i++) {
					val = mpcie_byte_read(addr);
					if (!decimal_format) {
						cli_print(ch, "%02X", val);
					} else {
						cli_print(ch, "%3u", val);
					}
					if (i + 1 < line_len) cli_print(ch, " ");
					if (!stream_access) addr++;
				}
				cli_print(ch, "\n");
				read_len -= line_len;
			}
		} else {
			for (uint32_t i = 0; i < read_len; i++) {
				val = mpcie_byte_read(addr);
				cli_print(ch, "%02X", val);
				if (!stream_access) addr++;
			}
			cli_print(ch, "\n");
		}	
	} else {
		for (uint32_t i = 0; i < data_len; i++) {
			mpcie_byte_write(addr, data[i]);
			if (!stream_access) addr++;
		}
	}

    return;
}

