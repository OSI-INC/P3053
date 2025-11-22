# Firmware for Embedded Ethernet Module (A3053A)

Copyright (C) 2025, Kevan Hashemi, Open Source Instruments Inc.  
Copyright (C) 2019 Microchip Technology Inc. and its subsidiaries.

## Introduction

The Embedded Ethernet Module (EEM) is a user-configurable TCP/IP server with its own Ethernet physical interface assembled on an mPCIe card. The mother board onto which the EEM mounts provides power, an RJ-45 connector, and a programming connector for the EEM's microcontroller. The microcontroller deployed on the A3053A EEM is the [PIC32MZ2048EFH-100](https://www.opensourceinstruments.com/Electronics/Data/PIC32MZ.pdf). The PIC32MZ2048EFH-100 is a 32-bit processor with 2 MByte of flash memory and 78 programmable input-output pins. On the A3053A, we run the processor at 200 MHz and most instructions take only one or two clock cycles to complete. 

![A3053A](https://www.opensourceinstruments.com/Electronics/A3053/HTML/A3053AV1.jpg)

The motivation behind our design and development of the EEM is to create a drop-in replacement for the Rabbit MiniCore [RCM6700](https://www.opensourceinstruments.com/Electronics/Data/RCM6700.pdf) module, which we first started using in 2011, and with which we were well-satisfied. But the RCM6700 was discontinued by its manufacturer in 2023 and all stocks of the modules are exhausted.


