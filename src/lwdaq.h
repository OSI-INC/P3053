/*
	lwdaq.c -- Interface of the Long-Wire Data Acquisition Relay library.
	Provides a LWDAQ server for a TCP socket, thus creating a LWDAQ Relay. Does
	not contain the routines that provide communication over the MCPIE parallel
	buss that connects the LWDAQ Relay to the Controller. Those routines are
	declared in pic.h.

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

#ifndef LWDAQ_H
#define LWDAQ_H

// Include files.
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>				  
#include <stdbool.h>
#include <stdlib.h>
#include <stdarg.h>
#include "configuration.h"
#include "definitions.h"
#include "utils.h"
#include "pic.h"
#include "console.h"
#include "server.h"

/*
	LWDAQ Controller register addresses on the MPCIE bus.
*/
#define LWDAQ_CONFIG_ADDR 40
#define LWDAQ_RESET_ADDR 41

/*
	LWDAQ Server routines and configuration.
*/
extern tcpip_server_type lwdaq_server;
int lwdaq_tasks(tcpip_server_type *s);

#endif

