/*
	pic.h -- Interface of the PIC32MZ Utility library for the Embedded Ethernet
	Module (EEM). Provides communication with the system hardware, including
	flash memory, the MPCIE parallel bus, the PIC32MZ internal registers, and
	the UART interfaces. Does not include control of the Ethernet physical
	layer, which you will find in server.h.

	Copyright (C) 2025-2026, Kevan Hashemi, Open Source Instruments Inc.

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

#ifndef PIC_H
#define PIC_H

/*
	We need the CLI declarations for the CLI channel types. These are used by the
	command procedures we declare below.
*/
#include "cli.h"

/*
	These routines turn on, turn off, and toggle D2 and D5. We declare them as
	static inline so they can be fast, which we like when we use these outputs
	as test points.
*/
static inline void pic_d2_on(void) {GPIO_PortSet(GPIO_PORT_C, 0x00008000);}
static inline void pic_d2_off(void) {GPIO_PortClear(GPIO_PORT_C, 0x00008000);}
static inline void pic_d2_toggle(void) {GPIO_PortToggle(GPIO_PORT_C, 0x00008000);}
static inline void pic_d5_on(void) {GPIO_PortSet(GPIO_PORT_A, 0x00000004);}
static inline void pic_d5_off(void) {GPIO_PortClear(GPIO_PORT_A, 0x00000004);}
static inline void pic_d5_toggle(void) {GPIO_PortToggle(GPIO_PORT_A, 0x00000004);}

/*
	We define masks for the MPCIE eight-bit parallel bus. This is the bus by
	which the EEM controls the motherboard. The EEM is master of this bus. The
	bus consists of the Control Address Bus (CA0-CA7), the Control Data Bus
	(DC0-CD7), Data Strobe (/CDS) and Control Write (/CWR). We define procedures
	to access the MPCIE parallel bus. All procedures are declared here as static
	and inline so they will be combined together and placed in our code without
	any actual function calls.
*/

// Control Address Bus, CAB, six bits CA0-CA5.
#define MPCIE_CA0_RC1      0x00000002u   // CA0=RC1
#define MPCIE_CA1_RC2      0x00000004u   // CA1=RC2
#define MPCIE_CA2_RC3      0x00000008u   // CA2=RC3
#define MPCIE_CA3_RC4      0x00000010u   // CA3=RC4
#define MPCIE_CA4_RE0      0x00000001u   // CA4=RE0
#define MPCIE_CA5_RE1      0x00000002u   // CA5=RE1
#define MPCIE_CA_MASK_RC   0x0000001Eu   // RC1–RC4 bits
#define MPCIE_CA_MASK_RE   0x00000003u   // RE0–RE1 bits

// Control Data Bus, CDB, eight bits CD0-CD7.
#define MPCIE_CD0_RC13   0x00002000u   // CD0=RC13
#define MPCIE_CD1_RC14   0x00004000u   // CD1=RC14
#define MPCIE_CD2_RE2    0x00000004u   // CD2=RE2
#define MPCIE_CD3_RE3    0x00000008u   // CD3=RE3
#define MPCIE_CD4_RE4    0x00000010u   // CD4=RE4
#define MPCIE_CD5_RE5    0x00000020u   // CD5=RE5
#define MPCIE_CD6_RE6    0x00000040u   // CD6=RE6
#define MPCIE_CD7_RE7    0x00000080u   // CD7=RE7
#define MPCIE_CD_MASK_RC 0x00006000u   // RC13-RC14
#define MPCIE_CD_MASK_RE 0x000000FCu   // RE2-RE7

// Control Data Strobe (/CDS) and Control Write (/CWR)
#define MPCIE_CDS_RD4     0x00000010u   // /CDS=RD4
#define MPCIE_CWR_RD5     0x00000020u   // /CWR=RD5

// Determine if we want the slower access cycles. This depends upon the
// motherboard.
#if defined(EEM_MOTHERBOARD_A3038) \
	|| defined(EEM_MOTHERBOARD_A3042)
#define MPCIE_SLOW_ACCESS
#endif


/*
	mpcie_wr_assert drives /CWR low to indicate an MPCIE bus write cycle. After
	we assert CWR, we can set the PIC controller data bus pins to be outputs. We
	assume the controller will turn off its line drivers immediately.
*/
static inline void mpcie_wr_assert(void) {
	LATDCLR = MPCIE_CWR_RD5;
}

/*
	mpcie_wr_unassert drives /CWR high to indicate a MPCIE bus read cycle. When
	we unassert CWR, the motherboard can drive the controller data bus lines.
*/
static inline void mpcie_wr_unassert(void) {
	LATDSET = MPCIE_CWR_RD5;
}

/* 
	mpcie_data_output sets the MPCIE data bus pins on the PIC as outputs. Must
	be preceeded by asserting Control Write (/CWR).
*/
static inline void mpcie_data_output(void) {
    TRISCCLR = MPCIE_CD_MASK_RC;   // RC13-RC14 as outputs
    TRISECLR = MPCIE_CD_MASK_RE;   // RE2–RE7 as outputs
}

/*
	mpcie_data_input sets the controller data bus pins on the PIC as inputs. Must
	preceed unasserting Controle Write (/CWR). We assume the PIC will turn off its
	line drivers immediately.
*/
static inline void mpcie_data_input(void) {
    TRISCSET = MPCIE_CD_MASK_RC;   // RC13, RC14 as inputs
    TRISESET = MPCIE_CD_MASK_RE;   // RE2–RE7 as inputs
}

/*
	mpcie_ds_assert drives asserts data strobe by driving the /CDS line low.
	Indicates that data is valid on a write cycle. Indicates that data is
	requested on a read cycle. We insert an access delay after CDS to allow time
	for the controller to respond. We make the delay out of PIC "nop"
	instructions, or "no operation" instructions. Each of these takes 5 ns for a
	PIC clock speed of 200 MHz, so each set of twenty is 100 ns. Our controllers
	interfaces run off either a 40-MHz or 80-MHz clock. We use hold time of 200
	ns and 100 ns ns respectively for these interfaces.
*/
static inline void mpcie_ds_assert(void)   {

	LATDCLR = MPCIE_CDS_RD4;

	__asm__ volatile (
		"nop\n\t"
		"nop\n\t"
		"nop\n\t"
		"nop\n\t"
		"nop\n\t"
		"nop\n\t"
		"nop\n\t"
		"nop\n\t"
		"nop\n\t"
		"nop\n\t"
		"nop\n\t"
		"nop\n\t"
		"nop\n\t"
		"nop\n\t"
		"nop\n\t"
		"nop\n\t"
		"nop\n\t"
		"nop\n\t"
		"nop\n\t"
		"nop\n\t"
     );    
     
#ifdef MPCIE_SLOW_ACCESS
	__asm__ volatile (
		"nop\n\t"
		"nop\n\t"
		"nop\n\t"
		"nop\n\t"
		"nop\n\t"
		"nop\n\t"
		"nop\n\t"
		"nop\n\t"
		"nop\n\t"
		"nop\n\t"
		"nop\n\t"
		"nop\n\t"
		"nop\n\t"
		"nop\n\t"
		"nop\n\t"
		"nop\n\t"
		"nop\n\t"
		"nop\n\t"
		"nop\n\t"
		"nop\n\t"
     );    
#endif

}

/*
	mpcie_ds_unassert drives the data strobe HI, which unasserts data strobe.
*/
static inline void mpcie_ds_unassert(void) {
	LATDSET = MPCIE_CDS_RD4;
}

/*
	mpcie_addr_set drives the mpcie address bus with the specified address. We
	insert an access delay after asserting the address lines to allow them to
	settle before we assert data strobe. On a read cycle, we unassert the mpcie
	write line and release the data lines before we call this routine, and in
	this way we the access delay applies to these changes as well. On a write,
	we set the data value and drive the data lines before we set the address, so
	as to allow them to settle as well. The delay is either 50 ns or 100 ns for
	normal and slow access respectively, as selected by a compiler flag.
*/
static inline void mpcie_addr_set(uint8_t addr) {
    LATC = (LATC & ~MPCIE_CA_MASK_RC) | ((uint32_t)(addr & 0x0Fu) << 1);
    LATE = (LATE & ~MPCIE_CA_MASK_RE) | ((uint32_t)(addr >> 4) & 0x03u);

	__asm__ volatile (
		"nop\n\t"
		"nop\n\t"
		"nop\n\t"
		"nop\n\t"
		"nop\n\t"
		"nop\n\t"
		"nop\n\t"
		"nop\n\t"
		"nop\n\t"
		"nop\n\t"
     );    
     
#ifdef MPCIE_SLOW_ACCESS
	__asm__ volatile (
		"nop\n\t"
		"nop\n\t"
		"nop\n\t"
		"nop\n\t"
		"nop\n\t"
		"nop\n\t"
		"nop\n\t"
		"nop\n\t"
		"nop\n\t"
		"nop\n\t"
     );    
#endif
}

/*
	mpcie_data_set assigns the mpcie interface data pins with the specified
	value. Note that this value will not appear on the data pins unless the pins
	are set as outputs. We have RC13 as bit0, RC14 as bit1, and RE2-RE7 as
	bit2-bit7. 
*/
static inline void mpcie_data_set(uint8_t data) {
	LATC = (LATC & ~MPCIE_CD_MASK_RC) | ((uint32_t) (data & 0x03u) << 13);
	LATE = (LATE & ~MPCIE_CD_MASK_RE) | ((uint32_t) (data & 0xFCu));
}

/*
	mpcie_data_get obtains the byte value on the mpcie interface data pins.
	This byte value is whatever is on the pins. We have to compose a byte from
	the various PIC pins we have assigned to the data bus, but this turns out to
	be straightforward: we take PORTC and shift it thirteen times to the right,
	at which point RC14 is in bit1 and RC13 is in bit0, so we AND it with 0x03. We
	take PORTE and we AND it immediately with 0xFC so that we get the bits
	RE2..RE7 in positions bit2..bit7. We OR these two bytes together and we have our
	value.
*/
static inline uint8_t mpcie_data_get(void) {
    uint8_t value = 0;
    uint32_t c = PORTC;
    uint32_t e = PORTE;
	value = (uint8_t) ((c >> 13) & 0x03u) | (e  & 0xFCu);
    return value;
}

/*
	mpcie_byte_write writes a byte value to a location in mpcie address space.
	It begins by setting the asserting the write line, setting the data value,
	and driving the data lines. It sets the address, which includes a settling
	delay. It drives data strobe, which includes an access delay to allow the
	mpcie to accept the data byte. It unasserts data strobe to end the write.
*/
static inline void mpcie_byte_write(uint8_t addr, uint8_t data) {
    mpcie_wr_assert(); 
    mpcie_data_set(data);
    mpcie_data_output();
    mpcie_addr_set(addr);
    mpcie_ds_assert();
	mpcie_ds_unassert();
}

/*
	mpcie_byte_write_repeat writes another byte to the same location in mpcie 
	address space. It sets the data lines, which includes a delay to let
	the data lines settle. It asserts data strobe, which includes a delay to let
	the write complete at the mpcie. It unasserts data strobe to end the
	cycle. It does not change the address, and it assumes that the write line
	has already been asserted. We use this routine for stream writes to the RAM
	Portal on LWDAQ Controllers.
*/
static inline void mpcie_byte_write_repeat(uint8_t addr, uint8_t data) {
	mpcie_data_set(data); 
	mpcie_ds_assert();
    mpcie_ds_unassert();
}

/*
	mpcie_byte_read reads a byte from a location in mpcie address space. It sets
	the data lines as inputs and unasserts the mpcie write line. It sets the
	address, which includes a settling delay. It asserts data strobe, which
	includes an access delay. It reads the byte from the data bus and unasserts
	data strobe.
*/
static inline uint8_t mpcie_byte_read(uint8_t addr) {
	uint8_t value;
	mpcie_data_input();
	mpcie_wr_unassert(); 
	mpcie_addr_set(addr);
	mpcie_ds_assert();
	value = mpcie_data_get();
	mpcie_ds_unassert();
	return value;
}

/*
	mpcie_byte_read_repeat reads again from the same location in mpcie address
	space. It assumes that the address lines, the write line, and the data lines
	are all configured for a read from this location. We do not have to wait for
	the address, write, or data lines to settle before asserting data strobe. We
	assert data strobe immediately, which includes an access delay, and then
	read the data lines. We use this routine in stream reads from the RAM Portal
	on LWDAQ controllers.
*/
static inline uint8_t mpcie_byte_read_repeat(void) {
	mpcie_ds_assert();
	uint8_t v = mpcie_data_get();
	mpcie_ds_unassert();
	return v;
}

/*
	Reading and writing from flash memory. These routines operate upon the flash
	memory page pointed to by their flash_addr argument. The write routines
	erases a page and then write either a byte array or a null-terminated string
	to the page, followed by 0xFF erase bytes. The read routines read from any
	location in flash memory, either a byte array or a string. When copying a
	string, if there is no null character to be found within the maximum copy
	length, the routine returns an error code. Otherwise the routines return the
	number of bytes read, or the length of the string read.

	The PIC32MZ2048EFH's 2 MByte of flash memory appears twice in the CPU's
	virtual address space. Once in the range 0x9D000000 to 0x9D0FFFFF, in which
	range the flash memory is accessed by the CPU indirectly through a cache
	memory, and again in the range 0xBD000000 to 0xBD0FFFFF, in which range the
	CPU accesses the flash memory directly, without a cache. We want to put our
	string in the un-cached copy so that we can write with the direct memory
	access (DMA) hardware and read back immediately from the physical flash
	memory rather than getting a stale copy of the flash memory from cache. So
	we place our string in the 0xBD range. In that range, we must make sure we
	are above the program itself. In our case, our program is less than 256
	KByte, so anywhere above 0xBD040000 will be fine. The flash memory pages are
	16 Kbyte in the PIC32MZ, and flash memory rows are 2 KByte. So we must pick
	a location that is on a 16-KByte boundary. Reading repeatedly from un-cached
	flash memory is far slower than reading repeatedly from a cached copy of an
	flash memory page, but we do not plan to read repeatedly from our
	configuration string, so we will suffer no loss of performance from reading
	direction from flash memory.
*/
int pic_flash_putbytes(uint32_t flash_addr, const uint8_t *buff, uint32_t len);
int pic_flash_writestr(uint32_t flash_addr, const char *str);
int pic_flash_getbytes(uint8_t *buff, uint32_t flash_addr, uint32_t len);
int pic_flash_readstr(char *str, uint32_t flash_addr, uint32_t str_size);

/*
	Reset, initialization, and configuration routines.
*/
void pic_reset(void);
void pic_initialize(void);
void pic_info(char *out);

/*
	Generic UART2 communication routines. Each of these takes a context pointer
	argument that allows it to be used with channel descriptors, such as our
	Command-Line Interpreter (CLI) uses to manage communication with a generic
	channel. These context pointers are not used by our UART2 routines, because
	the channel they use is always the same channel.
*/
int uart2_readcount(void *context);
int uart2_read(void *context, uint8_t *buff, uint32_t len);
int uart2_getchar(void *context);
int uart2_write(void *context, const uint8_t *buff, uint32_t len);
int uart2_putchar(void *context, char c);
int uart2_flush(void *context);

/*
	cli_pic_info is a CLI interface for PIC details.
*/
void cli_reset(cli_chan_type *ch, char *args);
#define CLI_MPCIE_MAX_DATA 512
void cli_mpcie(cli_chan_type *ch, char *args);

#endif
