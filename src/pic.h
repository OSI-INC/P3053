/*
	pic.h -- Interface of the PIC32MZ Utility library for the Embedded Ethernet
	Module (EEM). Provides communication with the system hardware, including
	non-volatile memory, the MPCIE parallel bus, the PIC32MZ internal registers,
	and the UART interfaces. Does not include control of the Ethernet physical
	layer, which you will find in server.h.

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

#ifndef PIC_H
#define PIC_H

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
	We define masks for the LWDAQ Controller interface address lines, data
	lines, and bus control lines. We define procedures to manipulate the LWDAQ
	Controller interface. All procedures are declared here as static and inline
	so they will be combined together and placed in our code without any actual
	function calls.
*/

// Control Address Bus, CAB, six bits CA0-CA5.
#define LWDAQ_CA0_RC1      0x00000002u   // CA0=RC1
#define LWDAQ_CA1_RC2      0x00000004u   // CA1=RC2
#define LWDAQ_CA2_RC3      0x00000008u   // CA2=RC3
#define LWDAQ_CA3_RC4      0x00000010u   // CA3=RC4
#define LWDAQ_CA4_RE0      0x00000001u   // CA4=RE0
#define LWDAQ_CA5_RE1      0x00000002u   // CA5=RE1
#define LWDAQ_CA_MASK_RC   0x0000001Eu   // RC1–RC4 bits
#define LWDAQ_CA_MASK_RE   0x00000003u   // RE0–RE1 bits

// Control Data Bus, CDB, eight bits CD0-CD7.
#define LWDAQ_CD0_RC13   0x00002000u   // CD0=RC13
#define LWDAQ_CD1_RC14   0x00004000u   // CD1=RC14
#define LWDAQ_CD2_RE2    0x00000004u   // CD2=RE2
#define LWDAQ_CD3_RE3    0x00000008u   // CD3=RE3
#define LWDAQ_CD4_RE4    0x00000010u   // CD4=RE4
#define LWDAQ_CD5_RE5    0x00000020u   // CD5=RE5
#define LWDAQ_CD6_RE6    0x00000040u   // CD6=RE6
#define LWDAQ_CD7_RE7    0x00000080u   // CD7=RE7
#define LWDAQ_CD_MASK_RC 0x00006000u   // RC13-RC14
#define LWDAQ_CD_MASK_RE 0x000000FCu   // RE2-RE7

// Control Data Strobe (/DS) and Control Write (/CW)
#define LWDAQ_DS_RD4     0x00000010u   // /DS=RD4
#define LWDAQ_CW_RD5     0x00000020u   // /CW=RD5

/*
	lwdaq_cw_assert drives /CW low to indicate a LWDAQ Controller write cycle. After
	we assert CW, we can set the PIC controller data bus pins to be outputs.
*/
static inline void lwdaq_cw_assert(void) {
	LATDCLR = LWDAQ_CW_RD5;
}

/*
	lwdaq_cw_unassert drives /CW high to indicate a LWDAQ Controller read cycle. When
	we unassert CW, the LWDAQ Controller can drive the controller data bus lines.
*/
static inline void lwdaq_cw_unassert(void) {
	LATDSET = LWDAQ_CW_RD5;
}

/* 
	lwdaq_data_output sets the controller data bus pins on the PIC as outputs.
	Must be preceeded by asserting Control Write (/CW).
*/
static inline void lwdaq_data_output(void) {
    TRISCCLR = LWDAQ_CD_MASK_RC;   // RC13-RC14 as outputs
    TRISECLR = LWDAQ_CD_MASK_RE;   // RE2–RE7 as outputs
}

/*
	lwdaq_data_input sets the controller data bus pins on the PIC as inputs. Must
	preceed unasserting Controle Write (/CW).
*/
static inline void lwdaq_data_input(void) {
    TRISCSET = LWDAQ_CD_MASK_RC;   // RC13, RC14 as inputs
    TRISESET = LWDAQ_CD_MASK_RE;   // RE2–RE7 as inputs
}

/*
	lwdaq_ds_assert drives the data strobe LO, which asserts data strobe.
	Indicates that data is valid on a write cycle. Indicates that data is
	requested on a read cycle. In both cases, we must leave DS asserted
	long enough for the controller to respond. 
*/
static inline void lwdaq_ds_assert(void)   {
	LATDCLR = LWDAQ_DS_RD4;
}

/*
	lwdaq_ds_unassert drives the data strobe HI, which unasserts data strobe.
*/
static inline void lwdaq_ds_unassert(void) {
	LATDSET = LWDAQ_DS_RD4;
}

/*
	lwdaq_addr_set drives the controller address bus with the specified address.
*/
static inline void lwdaq_addr_set(uint8_t addr) {
    // RC1..4 = bits 0..3
    LATC = (LATC & ~LWDAQ_CA_MASK_RC)
         | (((addr & 0x01) ? LWDAQ_CA0_RC1 : 0)
         |  ((addr & 0x02) ? LWDAQ_CA1_RC2 : 0)
         |  ((addr & 0x04) ? LWDAQ_CA2_RC3 : 0)
         |  ((addr & 0x08) ? LWDAQ_CA3_RC4 : 0));

    // RE0..1 = bits 4..5
    LATE = (LATE & ~LWDAQ_CA_MASK_RE)
         | (((addr & 0x10) ? LWDAQ_CA4_RE0 : 0)
         |  ((addr & 0x20) ? LWDAQ_CA5_RE1 : 0));
}

/*
	lwdaq_data_set assigns the controller interface data pins with the specified
	value. Note that this value will not appear on the data pins unless the pins
	are set as outputs. We have RC13 as bit0, RC14 as bit1, and RE2-RE7 as
	bit2-bit7.
*/
static inline void lwdaq_data_set(uint8_t data) {
    LATC = (LATC & ~LWDAQ_CD_MASK_RC)
         | (((data & 0x01) ? LWDAQ_CD0_RC13 : 0)
         |  ((data & 0x02) ? LWDAQ_CD1_RC14 : 0));
    LATE = (LATE & ~LWDAQ_CD_MASK_RE)
         | (((data & 0x04) ? LWDAQ_CD2_RE2 : 0)
         |  ((data & 0x08) ? LWDAQ_CD3_RE3 : 0)
         |  ((data & 0x10) ? LWDAQ_CD4_RE4 : 0)
         |  ((data & 0x20) ? LWDAQ_CD5_RE5 : 0)
         |  ((data & 0x40) ? LWDAQ_CD6_RE6 : 0)
         |  ((data & 0x80) ? LWDAQ_CD7_RE7 : 0));
}

/*
	lwdaq_data_get obtains the byte value on the controller interface data pins.
	This byte value is whatever is on the pins. We have to compose a byte from
	the various PIC pins we have assigned to the data bus.
*/
static inline uint8_t lwdaq_data_get(void) {
    uint8_t value = 0;
    uint32_t c = PORTC;
    uint32_t e = PORTE;

    if (c & LWDAQ_CD0_RC13) value |= 0x01;
    if (c & LWDAQ_CD1_RC14) value |= 0x02;
    if (e & LWDAQ_CD2_RE2)  value |= 0x04;
    if (e & LWDAQ_CD3_RE3)  value |= 0x08;
    if (e & LWDAQ_CD4_RE4)  value |= 0x10;
    if (e & LWDAQ_CD5_RE5)  value |= 0x20;
    if (e & LWDAQ_CD6_RE6)  value |= 0x40;
    if (e & LWDAQ_CD7_RE7)  value |= 0x80;

    return value;
}

/*
	lwdaq_byte_write writes a byte value to a location in control address space. It
	includes repeated calls to the data strobe assert routine in order to introduce
	a delay sufficient for the controller to latch the data accurately.
*/
static inline void lwdaq_byte_write(uint8_t addr, uint8_t data) {
    lwdaq_addr_set(addr);
    lwdaq_cw_assert(); 
    lwdaq_data_output();
    lwdaq_data_set(data);
    lwdaq_ds_assert();
    lwdaq_ds_assert();
    lwdaq_ds_assert();
    lwdaq_ds_assert();
    lwdaq_ds_assert();
    lwdaq_ds_unassert();
}

/*
	lwdaq_byte_read reads a byte from a location in control address space. It
	includes repeated calls to the data strobe assert routine to act as a delay
	sufficient for the controller to supply stable data. At the end of the
	routine we continue to drive the address bus with the same address, and we
	leave the control write line unasserted. We can follow with a repeat read
	access to read the same location again, which we do when reading from the
	controller's RAM Portal address.
*/
static inline uint8_t lwdaq_byte_read(uint8_t addr) {
    uint8_t value;
    lwdaq_addr_set(addr);
    lwdaq_data_input();
    lwdaq_cw_unassert(); 
    lwdaq_ds_assert();
    lwdaq_ds_assert();
    lwdaq_ds_assert();
    lwdaq_ds_assert();
    lwdaq_ds_assert();
    value = lwdaq_data_get();
    lwdaq_ds_unassert();
    return value;
}

/*
	lwdaq_byte_read_repeat reads again from the same byte read in a previous
	byte read or byte repeat read. This access is faster than the full byte read
	because we do not have to set up the address nor unassert control write
	(CW). We use this routine in stream reads from the controller RAM Portal.
*/
static inline uint8_t lwdaq_byte_read_repeat(void) {
    lwdaq_ds_assert();
    lwdaq_ds_assert();
    lwdaq_ds_assert();
    lwdaq_ds_assert();
    lwdaq_ds_assert();
    uint8_t v = lwdaq_data_get();
    lwdaq_ds_unassert();
    return v;
}

/*
	Reading and writing from non-volatile memory (NVM). These routines operate
	upon the flash memory page pointed to by their flash_addr argument. The
	write routines erases a page and then write either a byte array or a
	null-terminated string to the page, followed by 0xFF erase bytes. The read
	routines read from any location in NVM, either a byte array or a string.
	When copying a string, if there is no null character to be found within the
	maximum copy length, the routine returns an error code. Otherwise the
	routines return the number of bytes read, or the length of the string read.

	The PIC32MZ2048EFH's 2 MByte of NVM appears twice in the CPU's virtual
	address space. Once in the range 0x9D000000 to 0x9D0FFFFF, in which range
	the NVM is accessed by the CPU indirectly through a cache memory, and again
	in the range 0xBD000000 to 0xBD0FFFFF, in which range the CPU accesses the
	NVM directly, without a cache. We want to put our string in the un-cached
	copy so that we can write with the direct memory access (DMA) hardware and
	read back immediately from the physical NVM rather than getting a stale copy
	of the NVM from cache. So we place our string in the 0xBD range. In that
	range, we must make sure we are above the program itself. In our case, our
	program is less than 256 KByte, so anywhere above 0xBD040000 will be fine.
	The NVM pages are 16 Kbyte in the PIC32MZ, and NVM rows are 2 KByte. So we
	must pick a location that is on a 16-KByte boundary. Reading repeatedly from
	un-cached NVM is far slower than reading repeatedly from a cached copy of an
	NVM page, but we do not plan to read repeatedly from our configuration
	string, so we will suffer no loss of performance from reading direction from
	NVM.
*/
int pic_nvm_putbytes(uint32_t flash_addr, const uint8_t* buff, uint32_t len);
int pic_nvm_writestr(uint32_t flash_addr, const char* str);
int pic_nvm_getbytes(uint8_t* buff, uint32_t flash_addr, uint32_t len);
int pic_nvm_readstr(char* str, uint32_t flash_addr, uint32_t str_size);

/*
	Reset, initialization, and configuration routines.
*/
void pic_reset(void);
void pic_initialize(void);
void pic_info(char* out);

/*
	Generic UART2 communication routines. Each of these takes a context pointer
	argument that allows it to be used with channel descriptors, such as our
	command-line interpreter (CLI) uses to manage communication with a generic
	channel. These context pointers are not used by our UART2 routines, because
	the channel they use is always the same channel.
*/
int uart2_readcount(void *context);
int uart2_read(void *context, uint8_t* buff, uint32_t len);
int uart2_getchar(void *context);
int uart2_write(void *context, const uint8_t* buff, uint32_t len);
int uart2_putchar(void *context, char c);
int uart2_flush(void *context);

#endif
