/*
	utils.h declares variables, types, and functions used to interact with the
	system hardware, including the MPCIE connector parallel interface and the
	registers inside the PIC32MZ.
*/

#ifndef PIC_H
#define PIC_H

// Management of the PIC32MZ.
void pic_reset(void);
void pic_initialize(void);

#endif
