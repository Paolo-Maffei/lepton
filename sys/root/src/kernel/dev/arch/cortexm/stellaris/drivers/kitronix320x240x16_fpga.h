//*****************************************************************************
//
// kitronix320x240x16_FPGA.h - Prototypes for the Kitronix K350QVG-V1-F
//       display attached via the dk-lm3s9b96 FPGA/camera daughter board.
//
// Copyright (c) 2009-2011 Texas Instruments Incorporated.  All rights reserved.
// Software License Agreement
// 
// Texas Instruments (TI) is supplying this software for use solely and
// exclusively on TI's microcontroller products. The software is owned by
// TI and/or its suppliers, and is protected under applicable copyright
// laws. You may not combine this software with "viral" open-source
// software in order to form a larger program.
// 
// THIS SOFTWARE IS PROVIDED "AS IS" AND WITH ALL FAULTS.
// NO WARRANTIES, WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT
// NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. TI SHALL NOT, UNDER ANY
// CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL, OR CONSEQUENTIAL
// DAMAGES, FOR ANY REASON WHATSOEVER.
// 
// This is part of revision 6852 of the DK-LM3S9B96 Firmware Package.
//
//*****************************************************************************

#ifndef __KITRONIX320X240X16_FPGA_H__
#define __KITRONIX320X240X16_FPGA_H__

//*****************************************************************************
//
// Prototypes for the globals exported by this driver.
//
//*****************************************************************************
extern void Kitronix320x240x16_FPGAInit(unsigned long ulFrameBufAddr);
extern tDisplay g_sKitronix320x240x16_FPGA;
extern void Kitronix320x240x16_FPGASetLCDControl(unsigned char ucMask,
                                                    unsigned char ucVal);
extern unsigned long Kitronix320x240x16_FPGAColorMap(unsigned long ulValue);
extern void Kitronix320x240x16_FPGAModeSet(tBoolean bAutoRefresh);
extern void Kitronix320x240x16_FPGABacklight(tBoolean bEnable);

#endif // __KITRONIX320X240X16_FPGA_H__
