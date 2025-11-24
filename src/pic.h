/*
	utils.h declares variables, types, and functions used to interact with the
	system hardware, including the MPCIE connector parallel interface and the
	registers inside the PIC32MZ.
*/

#ifndef PIC_H
#define PIC_H

void pic_d2_on(void);
void pic_d2_off(void);
void pic_d2_toggle(void);
void pic_d5_on(void);
void pic_d5_off(void);
void pic_d5_toggle(void);
void pic_reset(void);
void pic_initialize(void);

// -------------------------
// Address bus bits (CA0–CA5)
// -------------------------

#define CA0_RC1   (1u << 1)     // RC1
#define CA1_RC2   (1u << 2)     // RC2
#define CA2_RC3   (1u << 3)     // RC3
#define CA3_RC4   (1u << 4)     // RC4
#define CA4_RE0   (1u << 0)     // RE0
#define CA5_RE1   (1u << 1)     // RE1
#define CA_MASK_RC   (CA0_RC1 | CA1_RC2 | CA2_RC3 | CA3_RC4)
#define CA_MASK_RE   (CA4_RE0 | CA5_RE1)


// -------------------------
// Data bus bits (CD0–CD7)
// -------------------------

#define CD0_RC13  (1u << 13)
#define CD1_RC14  (1u << 14)
#define CD2_RE2   (1u << 2)
#define CD3_RE3   (1u << 3)
#define CD4_RE4   (1u << 4)
#define CD5_RE5   (1u << 5)
#define CD6_RE6   (1u << 6)
#define CD7_RE7   (1u << 7)
#define CD_MASK_RC   (CD0_RC13 | CD1_RC14)
#define CD_MASK_RE   (CD2_RE2 | CD3_RE3 | CD4_RE4 | CD5_RE5 | CD6_RE6 | CD7_RE7)

// -------------------------
// Control lines
// -------------------------

#define DS_RD4    (1u << 4)       // /DS  active-low
#define CW_RD5    (1u << 5)       // /CW  active-low

static inline void cd_bus_as_outputs(void) {
    TRISCCLR = CD_MASK_RC;   // RC13, RC14 as outputs
    TRISECLR = CD_MASK_RE;   // RE2–RE7 as outputs
}

static inline void cd_bus_as_inputs(void) {
    TRISCSET = CD_MASK_RC;   // RC13, RC14 as inputs
    TRISESET = CD_MASK_RE;   // RE2–RE7 as inputs
}

static inline void ds_assert(void)   { LATDCLR = DS_RD4; }
static inline void ds_deassert(void) { LATDSET = DS_RD4; }
static inline void cw_assert(void)   { LATDCLR = CW_RD5; }
static inline void cw_deassert(void) { LATDSET = CW_RD5; }

static inline void bus_set_address(uint8_t addr) {
    // RC1..4 = bits 0..3
    LATC = (LATC & ~CA_MASK_RC)
         | (((addr & 0x01) ? CA0_RC1 : 0)
         |  ((addr & 0x02) ? CA1_RC2 : 0)
         |  ((addr & 0x04) ? CA2_RC3 : 0)
         |  ((addr & 0x08) ? CA3_RC4 : 0));

    // RE0..1 = bits 4..5
    LATE = (LATE & ~CA_MASK_RE)
         | (((addr & 0x10) ? CA4_RE0 : 0)
         |  ((addr & 0x20) ? CA5_RE1 : 0));
}

static inline void cd_write_bus(uint8_t data) {
    // RC13 = bit0, RC14 = bit1
    LATC = (LATC & ~CD_MASK_RC)
         | (((data & 0x01) ? CD0_RC13 : 0)
         |  ((data & 0x02) ? CD1_RC14 : 0));

    // RE2–RE7 = bits 2–7
    LATE = (LATE & ~CD_MASK_RE)
         | (((data & 0x04) ? CD2_RE2 : 0)
         |  ((data & 0x08) ? CD3_RE3 : 0)
         |  ((data & 0x10) ? CD4_RE4 : 0)
         |  ((data & 0x20) ? CD5_RE5 : 0)
         |  ((data & 0x40) ? CD6_RE6 : 0)
         |  ((data & 0x80) ? CD7_RE7 : 0));
}

static inline uint8_t cd_read_bus(void) {
    uint8_t value = 0;

    uint32_t c = PORTC;
    uint32_t e = PORTE;

    if (c & CD0_RC13) value |= 0x01;
    if (c & CD1_RC14) value |= 0x02;

    if (e & CD2_RE2)  value |= 0x04;
    if (e & CD3_RE3)  value |= 0x08;
    if (e & CD4_RE4)  value |= 0x10;
    if (e & CD5_RE5)  value |= 0x20;
    if (e & CD6_RE6)  value |= 0x40;
    if (e & CD7_RE7)  value |= 0x80;

    return value;
}

static inline void bus_write_byte(uint8_t addr, uint8_t data) {
    bus_set_address(addr);
    cw_assert(); 
    cd_bus_as_outputs();
    cd_write_bus(data);
    ds_assert();
    ds_assert();
    ds_assert();
    ds_assert();
    ds_assert();
    ds_deassert();
}

static inline uint8_t bus_read_byte(uint8_t addr) {
    uint8_t value;
    bus_set_address(addr);
    cd_bus_as_inputs();
    cw_deassert(); 
    ds_assert();
    ds_assert();
    ds_assert();
    ds_assert();
    ds_assert();
    value = cd_read_bus();
    ds_deassert();
    return value;
}

static inline uint8_t bus_read_same_byte(void) {
    ds_assert();
    // wait states
    uint8_t v = cd_read_bus();
    ds_deassert();
    return v;
}

#endif
