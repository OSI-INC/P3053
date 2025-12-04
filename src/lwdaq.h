/*
	lwdaq.h declares the variables, types, and functions used by our LWDAQ Relay to
	provide a LWDAQ Server. It does not include the routines used by LWDAQ Relay to 
	communicate with the LWDAQ Controller. Those routines are in our pic.c file.

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

#define LWDAQ_PORT 90
extern tcpip_server_type lwdaq_server;
int lwdaq_tasks(tcpip_server_type* s);

#endif

