/*
	config.h -- Embedded Ethernet Module Configuration Implemtation
	
	Here we have the global configuration flags that our application can change
	during run-time. The "debug" flag, for example, we can change from the
	Command-Line Interpreter at the console, and so enable or disable debug
	printing.

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

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>                  
#include <stdbool.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#include "config.h"

bool debug = EEM_DEBUG_DEFAULT;