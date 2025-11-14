/*
	utils.h declares variables, types, and functions used to interact with the
	system hardware, including the MPCIE connector parallel interface and the
	registers inside the PIC32MZ.
*/

#ifndef UTILS_H
#define UTILS_H

// String manipulation procedures.
const char* string_trim(const char *s);

// Management of the PIC32MZ.
void eem_reset(void);

// TCP/IP procedures.
void force_gateway_arp(void);
void ping_gateway(void);
void net_info(void);

#endif
