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

#endif
