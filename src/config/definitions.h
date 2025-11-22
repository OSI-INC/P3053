/*******************************************************************************
  System Definitions

  File Name:
    definitions.h

  Summary:
    project system definitions.

  Description:
    This file contains the system-wide prototypes and definitions for a project.
*******************************************************************************/

/*******************************************************************************
* Copyright (C) 2018 Microchip Technology Inc. and its subsidiaries.
*
* Subject to your compliance with these terms, you may use Microchip software
* and any derivatives exclusively with Microchip products. It is your
* responsibility to comply with third party license terms applicable to your
* use of third party software (including open source software) that may
* accompany Microchip software.
*
* THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES, WHETHER
* EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE, INCLUDING ANY IMPLIED
* WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS FOR A
* PARTICULAR PURPOSE.
*
* IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE,
* INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND
* WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP HAS
* BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE. TO THE
* FULLEST EXTENT ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL CLAIMS IN
* ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT OF FEES, IF ANY,
* THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS SOFTWARE.
 *******************************************************************************/

#ifndef DEFINITIONS_H
#define DEFINITIONS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "crypto/crypto.h"
#include "driver/ethmac/drv_ethmac.h"
#include "peripheral/plib_nvm.h"
#include "system/time/sys_time.h"
#include "peripheral/plib_coretimer.h"
#include "peripheral/plib_uart2.h"
#include "library/tcpip/tcpip.h"
#include "system/sys_time_h2_adapter.h"
#include "system/sys_random_h2_adapter.h"
#include "system/int/sys_int.h"
#include "system/reset/sys_reset.h"
#include "osal/osal.h"
#include "system/debug/sys_debug.h"
#include "system/command/sys_command.h"
#include "peripheral/plib_clk.h"
#include "peripheral/plib_gpio.h"
#include "peripheral/plib_evic.h"
#include "driver/miim/drv_miim.h"
#include "wolfssl/wolfcrypt/port/pic32/crypt_wolfcryptcb.h"
#include "system/console/sys_console.h"

// Device Information
#define DEVICE_NAME			 "PIC32MZ2048EFH100"
#define DEVICE_ARCH			 "MIPS"
#define DEVICE_FAMILY		 "PIC32MZEF"
#define DEVICE_SERIES		 "PIC32MZ"

// CPU clock frequency in Hz
#define CPU_CLOCK_FREQUENCY 200000000

<<<<<<< HEAD
// The TCP/IP Initialization routine.
=======
// Initialization routines we can call from our main program.
void ACCESS_Initialize (void);
void GPIO_Initialize (void);
void CONSOLE_Initialize (void);
void UTILS_Initialize (void);
>>>>>>> parent of e0a9ead (Rearranging initialization routines, compile broken.)
void TCPIP_Initialize (void);

// A record containing handles to data objects used by various processes.
typedef struct
{
	SYS_MODULE_OBJ  sysTime;
	SYS_MODULE_OBJ  tcpip;
	SYS_MODULE_OBJ  drvMiim_0;
	SYS_MODULE_OBJ  sysDebug;
} SYSTEM_OBJECTS;
extern SYSTEM_OBJECTS sysObj;

#endif
