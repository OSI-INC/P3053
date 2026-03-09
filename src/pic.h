/*
	pic.h -- Interface of the PIC32MZ library of routines for the Embedded
	Ethernet Module (EEM). Provides communication with the system hardware,
	including flash memory, the MPCIE parallel bus, the PIC32MZ internal
	registers, and the UART interfaces. Does not include control of the Ethernet
	physical layer, which you will find in server.h. In this interface, and also
	in the pic.c implementation file, you will see compiler macros that are
	defined in the interface file p32mz2048efh100.h, which is part of the
	PIC32MZ-EF_DFP device package. These macros define the addresses of PIC32
	registers. The macros use "LAT" for "LATCH" and "TRIS" for "Tri-State". Here
	are some examples.
	
	LATFSET Write 1 to bit N, port F bit N goes HI.
	LATACLR Write 1 to bit N, port A bit N goes LO.
	LATCINV Write 1 to bit N, port C bit N switches logic level.
    TRISECLR Write 1 to bit N, port E bit N becomes an output (not high impedance). 
    TRISASET Write 1 to bit N, port A bit N becomes an input (not high impedance). 
    
	Note that in all the above registers, writing a zero to a bit does nothing.
	They are registers that perform actions when we write a one to one or more
	of their bits. In our file structure, we have a Microchip directory next to
	our P3053 repository. Within the Microchip directory, we have a bunch of
	device packages. The one we use for the A3053 is:
	
	Microchip/PIC32MZ-EF_DFP/1.3.58/include/proc/p32mz2048efh100.h
	
	Copyright (C) 2025-2026, Kevan Hashemi, Open Source Instruments Inc.

	This program is free software: you can redistribute it and/or modify it
	under the terms of the GNU General Public License as published by the Free
	Software Foundation, either version 3 of the License, or (at your option)
	any later version.

	This program is distributed in the hope that it will be useful, but WITHOUT
	ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
	FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
	more details.

	You should have received a copy of the GNU General Public License along with
	this program. If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef PIC_H
#define PIC_H

/*
	We need the CLI declarations for the CLI channel types. These are used by the
	command procedures we declare below.
*/
#include "cli.h"

/*
	These routines turn on and of the available test point indicator lamps. We
	declare them as static inline so they can be fast, which we like when we use
	these outputs as test points.

	In the A3053A, lamps D2, D3, D4, and D5 are connected to RF3, RF2, RF8, and
	RA2 respectively. Lamps D3 and D4 are reserved for the TX and RX lines of
	UART2. For D3 and D4 we provide dummy routines. But we can use D2 and D5 as
	indicator lamps.

	In the A3053B, lamps D2, D3, D4, and D5 are connected to RF3, RF4, RF5, and
	RD9 respectively.
*/
#ifdef EEM_MODULE_A3053A
#define D2_MASK  0x00000008u  // RF3
#define D3_MASK  0x00000004u  // RF2
#define D4_MASK  0x00000100u  // RF8
#define D5_MASK  0x00000004u  // RA2
static inline void d2_on(void) {LATFSET = D2_MASK;}
static inline void d2_off(void) {LATFCLR = D2_MASK;}
static inline void d3_on(void) {;}
static inline void d3_off(void) {;}
static inline void d4_on(void) {;}
static inline void d4_off(void) {;}
static inline void d5_on(void) {LATASET = D5_MASK;}
static inline void d5_off(void) {LATACLR = D5_MASK;}
#endif

#ifdef EEM_MODULE_A3053B
#define D2_MASK    0x00000008u  // RF3
#define D3_MASK    0x00000010u  // RF4
#define D4_MASK    0x00000020u  // RF5
#define D5_MASK    0x00000200u  // RD9
static inline void d2_on(void) {LATFSET = D2_MASK;}
static inline void d2_off(void) {LATFCLR = D2_MASK;}
static inline void d3_on(void) {LATFSET = D3_MASK;}
static inline void d3_off(void) {LATFCLR = D3_MASK;}
static inline void d4_on(void) {LATFSET = D4_MASK;}
static inline void d4_off(void) {LATFCLR = D4_MASK;}
static inline void d5_on(void) {LATDSET = D5_MASK;}
static inline void d5_off(void) {LATDCLR = D5_MASK;}
#endif

/*
	We define masks for the mPCIe eight-bit parallel bus. This is the bus by
	which the EEM controls the motherboard. The EEM is master of this bus. The
	bus consists of the Control Address Bus (CA0-CA7), the Control Data Bus
	(DC0-CD7), Data Strobe (!CDS) and Control Write (!CWR). We define procedures
	to access the mPCIe parallel bus. All procedures are declared here as static
	and inline so they will be combined together and placed in our code without
	any actual function calls.

	In the A3053A we have the Control Address Bus (CAB) consisting of six bits
	CA0-CA5 on RC1-RC4, RE0-RE1. We have the Control Data Bus (CDB) consisting
	of eight bits CD0-CD7 on RC13, RC14, RE2-RE7. We have Control Data Strobe
	(!CDS) on RD4 and Control Write (!CWR) on RD5.

	In the A3053B we have the Control Address Bus (CAB) consisting of eight bits
	RE0-RE7. We have the Control Data Bus (CDB) consisting of eight bits
	RA0-RA7. We have Control Data Strobe (!CDS) on RA10 and Control Write (!CWR)
	on RA9.
*/

#ifdef EEM_MODULE_A3053A
#define MPCIE_CAB_MASK_RC   0x0000001Eu  // RC1–RC4 
#define MPCIE_CAB_MASK_RE   0x00000003u  // RE0–RE1 
#define MPCIE_CDB_MASK_RC   0x00006000u  // RC13-RC14
#define MPCIE_CDB_MASK_RE   0x000000FCu  // RE2-RE7
#define MPCIE_CDS_MASK      0x00000010u  // !CDS=RD4
#define MPCIE_CWR_MASK      0x00000020u  // !CWR=RD5
#endif

#ifdef EEM_MODULE_A3053B
#define MPCIE_CAB_MASK      0x000000FFu  // RE0-RE7
#define MPCIE_CDB_MASK      0x000000FFu  // RA0-RA7
#define MPCIE_CDS_MASK      0x00000400u  // !CDS=RA10
#define MPCIE_CWR_MASK      0x00000200u  // !CWR=RA9
#endif

// Some motherboards require a slower mPCIe access cycle. Here we set a slow
// access compiler flag for such motherboards.
#if defined(EEM_MOTHERBOARD_A3038) || defined(EEM_MOTHERBOARD_A3042)
#define MPCIE_SLOW_ACCESS
#endif

/*
	mpcie_wr_assert drives !CWR low to indicate an mPCIe bus write cycle. After
	we assert CWR, we can set the PIC controller data bus pins to be outputs. We
	assume the controller will turn off its line drivers immediately.
*/
static inline void mpcie_wr_assert(void) {

#ifdef EEM_MODULE_A3053A
	LATDCLR = MPCIE_CWR_MASK;
#endif

#ifdef EEM_MODULE_A3053B
	LATACLR = MPCIE_CWR_MASK;
#endif

}

/*
	mpcie_wr_unassert drives !CWR high to indicate a mPCIe bus read cycle. When
	we unassert CWR, the motherboard can drive the controller data bus lines.
*/
static inline void mpcie_wr_unassert(void) {

#ifdef EEM_MODULE_A3053A
	LATDSET = MPCIE_CWR_MASK;
#endif

#ifdef EEM_MODULE_A3053B
	LATASET = MPCIE_CWR_MASK;
#endif

}

/* 
	mpcie_data_output sets the mPCIe data bus pins on the PIC as outputs. Must
	be preceeded by asserting Control Write (!CWR).
*/
static inline void mpcie_data_output(void) {

#ifdef EEM_MODULE_A3053A
    TRISCCLR = MPCIE_CDB_MASK_RC;
    TRISECLR = MPCIE_CDB_MASK_RE;
#endif

#ifdef EEM_MODULE_A3053B
	TRISACLR = MPCIE_CDB_MASK;
#endif

}

/*
	mpcie_data_input sets the controller data bus pins on the PIC as inputs. Must
	preceed unasserting Controle Write (!CWR). We assume the PIC will turn off its
	line drivers immediately.
*/
static inline void mpcie_data_input(void) {

#ifdef EEM_MODULE_A3053A
	TRISCSET = MPCIE_CDB_MASK_RC; 
	TRISESET = MPCIE_CDB_MASK_RE;
#endif

#ifdef EEM_MODULE_A3053B
	TRISASET = MPCIE_CDB_MASK;
#endif

}

/*
	mpcie_ds_assert drives asserts data strobe by driving the !CDS line low.
	Indicates that data is valid on a write cycle. Indicates that data is
	requested on a read cycle. We insert an access delay after CDS to allow time
	for the controller to respond. We make the delay out of PIC "nop"
	operations. Each of these takes 5 ns for a PIC clock speed of 200 MHz, so
	each set of twenty is 100 ns. Our controllers interfaces run off either a
	40-MHz or 80-MHz clock. We use hold time of 200 ns and 150 ns ns
	respectively for these interfaces.
*/
static inline void mpcie_ds_assert(void)   {

#ifdef EEM_MODULE_A3053A
	LATDCLR = MPCIE_CDS_MASK;
#endif

#ifdef EEM_MODULE_A3053B
	LATACLR = MPCIE_CDS_MASK;
#endif

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
	mpcie_ds_unassert drives the data strobe HI, which unasserts data strobe.
*/
static inline void mpcie_ds_unassert(void) {

#ifdef EEM_MODULE_A3053A
	LATDSET = MPCIE_CDS_MASK;
#endif

#ifdef EEM_MODULE_A3053B
	LATASET = MPCIE_CDS_MASK;
#endif

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

#ifdef EEM_MODULE_A3053A
    LATC = (LATC & ~MPCIE_CAB_MASK_RC) | ((uint32_t)(addr & 0x0Fu) << 1);
    LATE = (LATE & ~MPCIE_CAB_MASK_RE) | ((uint32_t)(addr >> 4) & 0x03u);
#endif

#ifdef EEM_MODULE_A3053B
    LATE = (LATE & ~MPCIE_CAB_MASK) | ((uint32_t) (addr & 0xFFu));
#endif

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
	are set as outputs using the separate inline routine mpcie_data_output.
*/
static inline void mpcie_data_set(uint8_t data) {

#ifdef EEM_MODULE_A3053A
	LATC = (LATC & ~MPCIE_CDB_MASK_RC) | ((uint32_t) (data & 0x03u) << 13);
	LATE = (LATE & ~MPCIE_CDB_MASK_RE) | ((uint32_t) (data & 0xFCu));
#endif

#ifdef EEM_MODULE_A3053B
	LATA = (LATA & ~MPCIE_CDB_MASK) | ((uint32_t) (data & 0xFFu));
#endif

}

/*
	mpcie_data_get obtains the byte value on the mpcie interface data pins. This
	byte value is whatever is on the pins.
*/
static inline uint8_t mpcie_data_get(void) {
    
#ifdef EEM_MODULE_A3053A
    uint8_t value = 0;
	uint32_t c = PORTC;
	uint32_t e = PORTE;
	value = (uint8_t) ((c >> 13) & 0x03u) | (e  & 0xFCu);
    return value;
#endif

#ifdef EEM_MODULE_A3053B
	return (uint8_t) PORTA;	
#endif

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
void cli_lamp(cli_chan_type *ch, char *args);

#endif
