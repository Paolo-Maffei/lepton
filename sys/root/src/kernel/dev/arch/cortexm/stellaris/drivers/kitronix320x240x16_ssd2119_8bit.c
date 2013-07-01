//*****************************************************************************
//
// kitronix320x240x16_ssd2119_8bit.c - Display driver for the Kitronix
//                                     K350QVG-V1-F TFT display with an SSD2119
//                                     controller.  This version assumes an
//                                     8080-8bit interface between the micro
//                                     and display (PS3-0 = 0011b).
//
// Copyright (c) 2008-2011 Texas Instruments Incorporated.  All rights reserved.
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

//*****************************************************************************
//
//! \addtogroup display_api
//! @{
//
//*****************************************************************************

#include "inc/hw_gpio.h"
#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/gpio.h"
#include "driverlib/epi.h"
#include "driverlib/interrupt.h"
#include "driverlib/sysctl.h"
#include "driverlib/timer.h"
#include "grlib/grlib.h"
#include "drivers/kitronix320x240x16_ssd2119_8bit.h"
#include "drivers/set_pinout.h"
#include "drivers/camerafpga.h"

//*****************************************************************************
//
// This driver operates in four different screen orientations.  They are:
//
// * Portrait - The screen is taller than it is wide, and the flex connector is
//              on the left of the display.  This is selected by defining
//              PORTRAIT.
//
// * Landscape - The screen is wider than it is tall, and the flex connector is
//               on the bottom of the display.  This is selected by defining
//               LANDSCAPE.
//
// * Portrait flip - The screen is taller than it is wide, and the flex
//                   connector is on the right of the display.  This is
//                   selected by defining PORTRAIT_FLIP.
//
// * Landscape flip - The screen is wider than it is tall, and the flex
//                    connector is on the top of the display.  This is
//                    selected by defining LANDSCAPE_FLIP.
//
// These can also be imagined in terms of screen rotation; if portrait mode is
// 0 degrees of screen rotation, landscape is 90 degrees of counter-clockwise
// rotation, portrait flip is 180 degrees of rotation, and landscape flip is
// 270 degress of counter-clockwise rotation.
//
// If no screen orientation is selected, "landscape flip" mode will be used.
//
//*****************************************************************************
#if ! defined(PORTRAIT) && ! defined(PORTRAIT_FLIP) && \
    ! defined(LANDSCAPE) && ! defined(LANDSCAPE_FLIP)
#define LANDSCAPE_FLIP
#endif

//*****************************************************************************
//
// Various definitions controlling coordinate space mapping and drawing
// direction in the four supported orientations.
//
//*****************************************************************************
#ifdef PORTRAIT
#define HORIZ_DIRECTION 0x28
#define VERT_DIRECTION 0x20
#define MAPPED_X(x, y) (319 - (y))
#define MAPPED_Y(x, y) (x)
#endif
#ifdef LANDSCAPE
#define HORIZ_DIRECTION 0x00
#define VERT_DIRECTION  0x08
#define MAPPED_X(x, y) (319 - (x))
#define MAPPED_Y(x, y) (239 - (y))
#endif
#ifdef PORTRAIT_FLIP
#define HORIZ_DIRECTION 0x18
#define VERT_DIRECTION 0x10
#define MAPPED_X(x, y) (y)
#define MAPPED_Y(x, y) (239 - (x))
#endif
#ifdef LANDSCAPE_FLIP
#define HORIZ_DIRECTION 0x30
#define VERT_DIRECTION  0x38
#define MAPPED_X(x, y) (x)
#define MAPPED_Y(x, y) (y)
#endif

//*****************************************************************************
//
// Defines for the pins that are used to communicate with the SSD2119.
//
//*****************************************************************************

#define LCD_DATAH_PINS          0xFF
#define LCD_DATAH_PERIPH        SYSCTL_PERIPH_GPIOD
#define LCD_DATAH_BASE          GPIO_PORTD_BASE

//
// LCD control line GPIO definitions.
//
#define LCD_RST_PERIPH          SYSCTL_PERIPH_GPIOB
#define LCD_RST_BASE            GPIO_PORTB_BASE
#define LCD_RST_PIN             GPIO_PIN_7
#define LCD_RD_PERIPH           SYSCTL_PERIPH_GPIOB
#define LCD_RD_BASE             GPIO_PORTB_BASE
#define LCD_RD_PIN              GPIO_PIN_5
#define LCD_WR_PERIPH           SYSCTL_PERIPH_GPIOH
#define LCD_WR_BASE             GPIO_PORTH_BASE
#define LCD_WR_PIN              GPIO_PIN_6
#define LCD_DC_PERIPH           SYSCTL_PERIPH_GPIOH
#define LCD_DC_BASE             GPIO_PORTH_BASE
#define LCD_DC_PIN              GPIO_PIN_7

//*****************************************************************************
//
// Backlight control GPIO used with the Flash/SRAM/LCD daughter board.
//
//*****************************************************************************
#define LCD_BACKLIGHT_BASE      GPIO_PORTE_BASE
#define LCD_BACKLIGHT_PIN       GPIO_PIN_2

//*****************************************************************************
//
// Macro used to set the LCD data bus in preparation for writing a byte to the
// device.
//
//*****************************************************************************
#define SET_LCD_DATA(ucByte)                                                  \
{                                                                             \
    HWREG(LCD_DATAH_BASE + GPIO_O_DATA + (LCD_DATAH_PINS << 2)) = (ucByte);   \
}

//*****************************************************************************
//
// Various internal SD2119 registers name labels
//
//*****************************************************************************
#define SSD2119_DEVICE_CODE_READ_REG  0x00
#define SSD2119_OSC_START_REG         0x00
#define SSD2119_OUTPUT_CTRL_REG       0x01
#define SSD2119_LCD_DRIVE_AC_CTRL_REG 0x02
#define SSD2119_PWR_CTRL_1_REG        0x03
#define SSD2119_DISPLAY_CTRL_REG      0x07
#define SSD2119_FRAME_CYCLE_CTRL_REG  0x0B
#define SSD2119_PWR_CTRL_2_REG        0x0C
#define SSD2119_PWR_CTRL_3_REG        0x0D
#define SSD2119_PWR_CTRL_4_REG        0x0E
#define SSD2119_GATE_SCAN_START_REG   0x0F
#define SSD2119_SLEEP_MODE_1_REG      0x10
#define SSD2119_ENTRY_MODE_REG        0x11
#define SSD2119_SLEEP_MODE_2_REG      0x12
#define SSD2119_GEN_IF_CTRL_REG       0x15
#define SSD2119_PWR_CTRL_5_REG        0x1E
#define SSD2119_RAM_DATA_REG          0x22
#define SSD2119_FRAME_FREQ_REG        0x25
#define SSD2119_ANALOG_SET_REG        0x26
#define SSD2119_VCOM_OTP_1_REG        0x28
#define SSD2119_VCOM_OTP_2_REG        0x29
#define SSD2119_GAMMA_CTRL_1_REG      0x30
#define SSD2119_GAMMA_CTRL_2_REG      0x31
#define SSD2119_GAMMA_CTRL_3_REG      0x32
#define SSD2119_GAMMA_CTRL_4_REG      0x33
#define SSD2119_GAMMA_CTRL_5_REG      0x34
#define SSD2119_GAMMA_CTRL_6_REG      0x35
#define SSD2119_GAMMA_CTRL_7_REG      0x36
#define SSD2119_GAMMA_CTRL_8_REG      0x37
#define SSD2119_GAMMA_CTRL_9_REG      0x3A
#define SSD2119_GAMMA_CTRL_10_REG     0x3B
#define SSD2119_V_RAM_POS_REG         0x44
#define SSD2119_H_RAM_START_REG       0x45
#define SSD2119_H_RAM_END_REG         0x46
#define SSD2119_X_RAM_ADDR_REG        0x4E
#define SSD2119_Y_RAM_ADDR_REG        0x4F

#define ENTRY_MODE_DEFAULT 0x6830
#define MAKE_ENTRY_MODE(x) ((ENTRY_MODE_DEFAULT & 0xFF00) | (x))

//*****************************************************************************
//
// The dimensions of the LCD panel.
//
//*****************************************************************************
#define LCD_VERTICAL_MAX 240
#define LCD_HORIZONTAL_MAX 320

//*****************************************************************************
//
// Translates a 24-bit RGB color to a display driver-specific color.
//
// \param c is the 24-bit RGB color.  The least-significant byte is the blue
// channel, the next byte is the green channel, and the third byte is the red
// channel.
//
// This macro translates a 24-bit RGB color into a value that can be written
// into the display's frame buffer in order to reproduce that color, or the
// closest possible approximation of that color.
//
// \return Returns the display-driver specific color.
//
//*****************************************************************************
#define DPYCOLORTRANSLATE(c)    ((((c) & 0x00f80000) >> 8) |               \
                                 (((c) & 0x0000fc00) >> 5) |               \
                                 (((c) & 0x000000f8) >> 3))

//*****************************************************************************
//
// Function pointer types for low level LCD controller access functions.
//
//*****************************************************************************
typedef void (*pfnWriteData)(unsigned short usData);
typedef void (*pfnWriteCommand)(unsigned char ucData);

//*****************************************************************************
//
// Function pointers for low level LCD controller access functions.
//
//*****************************************************************************

static void WriteDataGPIO(unsigned short usData);
static void WriteCommandGPIO(unsigned char ucData);

//#define WriteData WriteDataGPIO
pfnWriteData WriteData = WriteDataGPIO;
//#define WriteCommand WriteCommandGPIO
pfnWriteCommand WriteCommand = WriteCommandGPIO;

//*****************************************************************************
//
// Writes a data word to the SSD2119.  This function implements the basic GPIO
// interface to the LCD display.
//
//*****************************************************************************
static void
WriteDataGPIO(unsigned short usData)
{
    //
    // Write the most significant byte of the data to the bus.
    //
    SET_LCD_DATA(usData >> 8);

    //
    // Assert the write enable signal.  We need to do this 2 times to ensure
    // that we don't violate the timing requirements for the display.
    //
    HWREG(LCD_WR_BASE + GPIO_O_DATA + (LCD_WR_PIN << 2)) = 0;
    HWREG(LCD_WR_BASE + GPIO_O_DATA + (LCD_WR_PIN << 2)) = 0;

    //
    // Deassert the write enable signal.
    //
    HWREG(LCD_WR_BASE + GPIO_O_DATA + (LCD_WR_PIN << 2)) = LCD_WR_PIN;

    //
    // Write the least significant byte of the data to the bus.
    //
    SET_LCD_DATA(usData);

    //
    // Assert the write enable signal.
    //
    HWREG(LCD_WR_BASE + GPIO_O_DATA + (LCD_WR_PIN << 2)) = 0;
    HWREG(LCD_WR_BASE + GPIO_O_DATA + (LCD_WR_PIN << 2)) = 0;

    //
    // Deassert the write enable signal.
    //
    HWREG(LCD_WR_BASE + GPIO_O_DATA + (LCD_WR_PIN << 2)) = LCD_WR_PIN;
}

//*****************************************************************************
//
// Writes a data word to the SSD2119 via the EPI interface as wired when using
// the lm3s9b96 development kit SRAM/flash daughter board.
//
//*****************************************************************************
static void
WriteDataEPI(unsigned short usData)
{
    HWREGB(LCD_DATA_PORT) = usData >> 8;
    HWREGB(LCD_DATA_PORT) = usData;
}

//*****************************************************************************
//
// Writes a command word to the SSD2119 via the EPI interface as wired when
// using the lm3s9b96 development kit SRAM/flash daughter board.
//
//*****************************************************************************
static void
WriteCommandEPI(unsigned char ucData)
{
    HWREGB(LCD_COMMAND_PORT) = 0;
    HWREGB(LCD_COMMAND_PORT) = ucData;
}

//*****************************************************************************
//
// Writes a data word to the SSD2119 via the EPI interface as wired when using
// the lm3s9b96 development kit SRAM/flash daughter board.
//
//*****************************************************************************
static void
WriteDataFPGA(unsigned short usData)
{
    HWREGH(LCD_FPGA_DATA_PORT) = usData;
    SysCtlDelay(1);
}

//*****************************************************************************
//
// Writes a command word to the SSD2119 via the EPI interface as wired when
// using the lm3s9b96 development kit SRAM/flash daughter board.
//
//*****************************************************************************
static void
WriteCommandFPGA(unsigned char ucData)
{
    HWREGH(LCD_FPGA_COMMAND_PORT) = (unsigned short)ucData;
    SysCtlDelay(1);
}

//*****************************************************************************
//
// Writes a command to the SSD2119.  This function implements the basic GPIO
// interface to the LCD display.
//
//*****************************************************************************
static void
WriteCommandGPIO(unsigned char ucData)
{
    //
    // Write the most significant byte of the data to the bus.  This is always
    // 0 since commands are no more than 8 bits.
    //
    SET_LCD_DATA(0);

    //
    // Assert DC
    //
    HWREG(LCD_DC_BASE + GPIO_O_DATA + (LCD_DC_PIN << 2)) = 0;

    //
    // Assert the write enable signal.  We need to do this 2 times to ensure
    // that we don't violate the timing requirements for the display.
    //
    HWREG(LCD_WR_BASE + GPIO_O_DATA + (LCD_WR_PIN << 2)) = 0;
    HWREG(LCD_WR_BASE + GPIO_O_DATA + (LCD_WR_PIN << 2)) = 0;

    //
    // Deassert the write enable signal.
    //
    HWREG(LCD_WR_BASE + GPIO_O_DATA + (LCD_WR_PIN << 2)) = LCD_WR_PIN;

    //
    // Write the least significant byte of the data to the bus.
    //
    SET_LCD_DATA(ucData);

    //
    // Assert the write enable signal.
    //
    HWREG(LCD_WR_BASE + GPIO_O_DATA + (LCD_WR_PIN << 2)) = 0;
    HWREG(LCD_WR_BASE + GPIO_O_DATA + (LCD_WR_PIN << 2)) = 0;

    //
    // Deassert the write enable signal.
    //
    HWREG(LCD_WR_BASE + GPIO_O_DATA + (LCD_WR_PIN << 2)) = LCD_WR_PIN;

    //
    // Set the DC signal high, indicating that following writes are data.
    //
    HWREG(LCD_DC_BASE + GPIO_O_DATA + (LCD_DC_PIN << 2)) = LCD_DC_PIN;
}

//*****************************************************************************
//
// Initializes the pins required for the GPIO-based LCD interface.
//
// This function configures the GPIO pins used to control the LCD display
// when the basic GPIO interface is in use.  On exit, the LCD controller
// has been reset and is ready to receive command and data writes.
//
// \return None.
//
//*****************************************************************************
static void
InitGPIOLCDInterface(unsigned long ulClockMS)
{
    WriteData = WriteDataGPIO;      //GD
    WriteCommand = WriteCommandGPIO;//GD
    //
    // Convert the PB7/NMI pin into a GPIO pin.  This requires the use of the
    // GPIO lock since changing the state of the pin is otherwise disabled.
    //
    HWREG(GPIO_PORTB_BASE + GPIO_O_LOCK) = GPIO_LOCK_KEY_DD;
    HWREG(GPIO_PORTB_BASE + GPIO_O_CR) = 0x80;

    //
    // Make PB7 an output.
    //
    GPIODirModeSet(GPIO_PORTB_BASE, GPIO_PIN_7, GPIO_DIR_MODE_OUT);
    GPIOPadConfigSet(GPIO_PORTB_BASE, GPIO_PIN_7, GPIO_STRENGTH_8MA,
                     GPIO_PIN_TYPE_STD);

    //
    // Clear the commit register, effectively locking access to registers
    // controlling the PB7 configuration.
    //
    HWREG(GPIO_PORTB_BASE + GPIO_O_LOCK) = GPIO_LOCK_KEY_DD;
    HWREG(GPIO_PORTB_BASE + GPIO_O_CR) = 0x00;

    //
    // Configure the pins that connect to the LCD as GPIO outputs.
    //
    GPIODirModeSet(LCD_DATAH_BASE, LCD_DATAH_PINS, GPIO_DIR_MODE_OUT);
    GPIOPadConfigSet(LCD_DATAH_BASE, LCD_DATAH_PINS, GPIO_STRENGTH_8MA,
                     GPIO_PIN_TYPE_STD);
    GPIODirModeSet(LCD_DC_BASE, LCD_DC_PIN, GPIO_DIR_MODE_OUT);
    GPIOPadConfigSet(LCD_DC_BASE, LCD_DC_PIN, GPIO_STRENGTH_8MA,
                     GPIO_PIN_TYPE_STD);
    GPIODirModeSet(LCD_RD_BASE, LCD_RD_PIN, GPIO_DIR_MODE_OUT);
    GPIOPadConfigSet(LCD_RD_BASE, LCD_RD_PIN, GPIO_STRENGTH_8MA,
                     GPIO_PIN_TYPE_STD);
    GPIODirModeSet(LCD_WR_BASE, LCD_WR_PIN, GPIO_DIR_MODE_OUT);
    GPIOPadConfigSet(LCD_WR_BASE, LCD_WR_PIN, GPIO_STRENGTH_8MA,
                     GPIO_PIN_TYPE_STD);
    GPIODirModeSet(LCD_RST_BASE, LCD_RST_PIN, GPIO_DIR_MODE_OUT);
    GPIOPadConfigSet(LCD_RST_BASE, LCD_RST_PIN, GPIO_STRENGTH_8MA,
                     GPIO_PIN_TYPE_STD);

    //
    // Set the LCD control pins to their default values.  This also asserts the
    // LCD reset signal.
    //
    GPIOPinWrite(LCD_DATAH_BASE, LCD_DATAH_PINS, 0x00);
    GPIOPinWrite(LCD_DC_BASE, LCD_DC_PIN, 0x00);
    GPIOPinWrite(LCD_RD_BASE, LCD_RD_PIN, LCD_RD_PIN);
    GPIOPinWrite(LCD_WR_BASE, LCD_WR_PIN, LCD_WR_PIN);
    GPIOPinWrite(LCD_RST_BASE, LCD_RST_PIN, 0x00);

    //
    // Delay for 1ms.
    //
    SysCtlDelay(ulClockMS);

    //
    // Deassert the LCD reset signal.
    //
    GPIOPinWrite(LCD_RST_BASE, LCD_RST_PIN, LCD_RST_PIN);

    //
    // Delay for 1ms while the LCD comes out of reset.
    //
    SysCtlDelay(ulClockMS);
}

//*****************************************************************************
//
// Initialize the low level LCD communication when the SRAM/Flash daughter
// board is connected.
//
// This function sets up the low level LCD access function pointers correctly
// when the SRAM/Flash daughter board is present.  In this case, communication
// with the LCD panel is via the EPI rather than via the basic GPIO interface.
// On exit, the LCD controller has been reset and is ready to receive command
// and data writes.
//
// \return None.
//
//*****************************************************************************
static void
InitSRAMDaughterLCDInterface(unsigned long ulClockMS)
{
    //
    // Set the low level access function pointers so that the EPI interface is
    // used to access the LCD controller.
    //
    WriteData = WriteDataEPI;
    WriteCommand = WriteCommandEPI;

    //
    // Asserts the LCD reset signal.
    //
    HWREGB(LCD_CONTROL_CLR_REG) = LCD_CONTROL_NRESET;

    //
    // Delay for 1ms.
    //
    SysCtlDelay(ulClockMS);

    //
    // Deassert the LCD reset signal
    //
    HWREGB(LCD_CONTROL_SET_REG) = LCD_CONTROL_NRESET;

    //
    // Delay for 1ms while the LCD comes out of reset.
    //
    SysCtlDelay(ulClockMS);

    //
    // Turn on the backlight.
    //
    GPIOPinTypeGPIOOutput(LCD_BACKLIGHT_BASE, LCD_BACKLIGHT_PIN);
    GPIOPinWrite(LCD_BACKLIGHT_BASE, LCD_BACKLIGHT_PIN, LCD_BACKLIGHT_PIN);
}

//*****************************************************************************
//
// Initialize the low level LCD communication when the FPGA/Camera/LCD
// daughter board is connected.
//
// This function sets up the low level LCD access function pointers correctly
// when the SRAM/Flash daughter board is present.  In this case, communication
// with the LCD panel is via the EPI rather than via the basic GPIO interface.
// On exit, the LCD controller has been reset and is ready to receive command
// and data writes.
//
// \return None.
//
//*****************************************************************************
static void
InitFPGADaughterLCDInterface(unsigned long ulClockMS)
{
    //
    // Set the low level access function pointers so that the EPI interface as
    // configured for the FPGA/Camera/LCD daughter board is used to
    // access the LCD controller.
    //
    WriteData = WriteDataFPGA;
    WriteCommand = WriteCommandFPGA;

    //
    // Asserts the LCD reset signal.
    //
    HWREGH(LCD_FPGA_CONTROL_CLR_REG) = LCD_CONTROL_NRESET;

    //
    // Delay for 1ms.
    //
    SysCtlDelay(ulClockMS);

    //
    // Deassert the LCD reset signal
    //
    HWREGH(LCD_FPGA_CONTROL_SET_REG) = LCD_CONTROL_NRESET;

    //
    // Delay for 1ms while the LCD comes out of reset.
    //
    SysCtlDelay(ulClockMS);

    //
    // Enable the LCD backlight.
    //
    HWREGH(FPGA_LCDSET_REG) |= LCD_CONTROL_BKLIGHT;
}

//*****************************************************************************
//
//! Initializes the display driver.
//!
//! This function initializes the SSD2119 display controller on the panel,
//! preparing it to display data.
//!
//! \return None.
//
//*****************************************************************************
void
Kitronix320x240x16_SSD2119Init(void)
{
    unsigned long ulClockMS, ulCount;

    //
    // Get the current processor clock frequency.
    //
    ulClockMS = SysCtlClockGet() / (3 * 1000);

    //
    // Perform low level interface initialization depending upon how the LCD
    // is connected to the Stellaris microcontroller.  This varies depending
    // upon the daughter board connected it is possible that a daughter board
    // can drive the LCD directly rather than via the basic GPIO interface.
    //
    switch(g_eDaughterType)
    {
        case DAUGHTER_NONE:
        default:
        {
            //
            // Initialize the GPIOs used to interface to the LCD controller.
            //
            InitGPIOLCDInterface(ulClockMS);
            break;
        }

        case DAUGHTER_SRAM_FLASH:
        {
            //
            // Initialize the display via the EPI interface to the LCD.
            //
            InitSRAMDaughterLCDInterface(ulClockMS);
            break;
        }

        case DAUGHTER_FPGA:
        {
            //
            // Initialize the display via the EPI interface to the LCD.
            //
            InitFPGADaughterLCDInterface(ulClockMS);
            break;
        }
    }

    //
    // Enter sleep mode (if we are not already there).
    //
    WriteCommand(SSD2119_SLEEP_MODE_1_REG);
    WriteData(0x0001);

    //
    // Set initial power parameters.
    //
    WriteCommand(SSD2119_PWR_CTRL_5_REG);
    WriteData(0x00B2);
    WriteCommand(SSD2119_VCOM_OTP_1_REG);
    WriteData(0x0006);

    //
    // Start the oscillator.
    //
    WriteCommand(SSD2119_OSC_START_REG);
    WriteData(0x0001);

    //
    // Set pixel format and basic display orientation (scanning direction).
    //
    WriteCommand(SSD2119_OUTPUT_CTRL_REG);
    WriteData(0x30EF);
    WriteCommand(SSD2119_LCD_DRIVE_AC_CTRL_REG);
    WriteData(0x0600);

    //
    // Exit sleep mode.
    //
    WriteCommand(SSD2119_SLEEP_MODE_1_REG);
    WriteData(0x0000);

    //
    // Delay 30mS
    //
    SysCtlDelay(30 * ulClockMS);

    //
    // Configure pixel color format and MCU interface parameters.
    //
    WriteCommand(SSD2119_ENTRY_MODE_REG);
    WriteData(ENTRY_MODE_DEFAULT);

    //
    // Set analog parameters.
    //
    WriteCommand(SSD2119_SLEEP_MODE_2_REG);
    WriteData(0x0999);
    WriteCommand(SSD2119_ANALOG_SET_REG);
    WriteData(0x3800);

    //
    // Enable the display.
    //
    WriteCommand(SSD2119_DISPLAY_CTRL_REG);
    WriteData(0x0033);

    //
    // Set VCIX2 voltage to 6.1V.
    //
    WriteCommand(SSD2119_PWR_CTRL_2_REG);
    WriteData(0x0005);

    //
    // Configure gamma correction.
    //
    WriteCommand(SSD2119_GAMMA_CTRL_1_REG);
    WriteData(0x0000);
    WriteCommand(SSD2119_GAMMA_CTRL_2_REG);
    WriteData(0x0303);
    WriteCommand(SSD2119_GAMMA_CTRL_3_REG);
    WriteData(0x0407);
    WriteCommand(SSD2119_GAMMA_CTRL_4_REG);
    WriteData(0x0301);
    WriteCommand(SSD2119_GAMMA_CTRL_5_REG);
    WriteData(0x0301);
    WriteCommand(SSD2119_GAMMA_CTRL_6_REG);
    WriteData(0x0403);
    WriteCommand(SSD2119_GAMMA_CTRL_7_REG);
    WriteData(0x0707);
    WriteCommand(SSD2119_GAMMA_CTRL_8_REG);
    WriteData(0x0400);
    WriteCommand(SSD2119_GAMMA_CTRL_9_REG);
    WriteData(0x0a00);
    WriteCommand(SSD2119_GAMMA_CTRL_10_REG);
    WriteData(0x1000);
    //
    // Configure Vlcd63 and VCOMl.
    //
    WriteCommand(SSD2119_PWR_CTRL_3_REG);
    WriteData(0x000A);
    WriteCommand(SSD2119_PWR_CTRL_4_REG);
    WriteData(0x2E00);

    //
    // Set the display size and ensure that the GRAM window is set to allow
    // access to the full display buffer.
    //
    WriteCommand(SSD2119_V_RAM_POS_REG);
    WriteData((LCD_VERTICAL_MAX-1) << 8);
    WriteCommand(SSD2119_H_RAM_START_REG);
    WriteData(0x0000);
    WriteCommand(SSD2119_H_RAM_END_REG);
    WriteData(LCD_HORIZONTAL_MAX-1);
    WriteCommand(SSD2119_X_RAM_ADDR_REG);
    WriteData(0x00);
    WriteCommand(SSD2119_Y_RAM_ADDR_REG);
    WriteData(0x00);

    //
    // Clear the contents of the display buffer.
    //
    WriteCommand(SSD2119_RAM_DATA_REG);
    for(ulCount = 0; ulCount < (320 * 240); ulCount++)
    {
        WriteData(0x0000);
    }
}

//*****************************************************************************
//
//! Draws a pixel on the screen.
//!
//! \param pvDisplayData is a pointer to the driver-specific data for this
//! display driver.
//! \param lX is the X coordinate of the pixel.
//! \param lY is the Y coordinate of the pixel.
//! \param ulValue is the color of the pixel.
//!
//! This function sets the given pixel to a particular color.  The coordinates
//! of the pixel are assumed to be within the extents of the display.
//!
//! \return None.
//
//*****************************************************************************
static void
Kitronix320x240x16_SSD2119PixelDraw(void *pvDisplayData, long lX, long lY,
                                   unsigned long ulValue)
{
    //
    // Set the X address of the display cursor.
    //
    WriteCommand(SSD2119_X_RAM_ADDR_REG);
    WriteData(MAPPED_X(lX, lY));

    //
    // Set the Y address of the display cursor.
    //
    WriteCommand(SSD2119_Y_RAM_ADDR_REG);
    WriteData(MAPPED_Y(lX, lY));

    //
    // Write the pixel value.
    //
    WriteCommand(SSD2119_RAM_DATA_REG);
    WriteData(ulValue);
}

//*****************************************************************************
//
//! Draws a horizontal sequence of pixels on the screen.
//!
//! \param pvDisplayData is a pointer to the driver-specific data for this
//! display driver.
//! \param lX is the X coordinate of the first pixel.
//! \param lY is the Y coordinate of the first pixel.
//! \param lX0 is sub-pixel offset within the pixel data, which is valid for 1
//! or 4 bit per pixel formats.
//! \param lCount is the number of pixels to draw.
//! \param lBPP is the number of bits per pixel; must be 1, 4, or 8.
//! \param pucData is a pointer to the pixel data.  For 1 and 4 bit per pixel
//! formats, the most significant bit(s) represent the left-most pixel.
//! \param pucPalette is a pointer to the palette used to draw the pixels.
//!
//! This function draws a horizontal sequence of pixels on the screen, using
//! the supplied palette.  For 1 bit per pixel format, the palette contains
//! pre-translated colors; for 4 and 8 bit per pixel formats, the palette
//! contains 24-bit RGB values that must be translated before being written to
//! the display.
//!
//! \return None.
//
//*****************************************************************************
static void
Kitronix320x240x16_SSD2119PixelDrawMultiple(void *pvDisplayData, long lX,
                                           long lY, long lX0, long lCount,
                                           long lBPP,
                                           const unsigned char *pucData,
                                           const unsigned char *pucPalette)
{
    unsigned long ulByte;

    //
    // Set the cursor increment to left to right, followed by top to bottom.
    //
    WriteCommand(SSD2119_ENTRY_MODE_REG);
    WriteData(MAKE_ENTRY_MODE(HORIZ_DIRECTION));

    //
    // Set the starting X address of the display cursor.
    //
    WriteCommand(SSD2119_X_RAM_ADDR_REG);
    WriteData(MAPPED_X(lX, lY));

    //
    // Set the Y address of the display cursor.
    //
    WriteCommand(SSD2119_Y_RAM_ADDR_REG);
    WriteData(MAPPED_Y(lX, lY));

    //
    // Write the data RAM write command.
    //
    WriteCommand(SSD2119_RAM_DATA_REG);

    //
    // Determine how to interpret the pixel data based on the number of bits
    // per pixel.
    //
    switch(lBPP)
    {
        //
        // The pixel data is in 1 bit per pixel format.
        //
        case 1:
        {
            //
            // Loop while there are more pixels to draw.
            //
            while(lCount)
            {
                //
                // Get the next byte of image data.
                //
                ulByte = *pucData++;

                //
                // Loop through the pixels in this byte of image data.
                //
                for(; (lX0 < 8) && lCount; lX0++, lCount--)
                {
                    //
                    // Draw this pixel in the appropriate color.
                    //
                    WriteData(((unsigned long *)pucPalette)[(ulByte >>
                                                             (7 - lX0)) & 1]);
                }

                //
                // Start at the beginning of the next byte of image data.
                //
                lX0 = 0;
            }

            //
            // The image data has been drawn.
            //
            break;
        }

        //
        // The pixel data is in 4 bit per pixel format.
        //
        case 4:
        {
            //
            // Loop while there are more pixels to draw.  "Duff's device" is
            // used to jump into the middle of the loop if the first nibble of
            // the pixel data should not be used.  Duff's device makes use of
            // the fact that a case statement is legal anywhere within a
            // sub-block of a switch statement.  See
            // http://en.wikipedia.org/wiki/Duff's_device for detailed
            // information about Duff's device.
            //
            switch(lX0 & 1)
            {
                case 0:
                    while(lCount)
                    {
                        //
                        // Get the upper nibble of the next byte of pixel data
                        // and extract the corresponding entry from the
                        // palette.
                        //
                        ulByte = (*pucData >> 4) * 3;
                        ulByte = (*(unsigned long *)(pucPalette + ulByte) &
                                  0x00ffffff);

                        //
                        // Translate this palette entry and write it to the
                        // screen.
                        //
                        WriteData(DPYCOLORTRANSLATE(ulByte));

                        //
                        // Decrement the count of pixels to draw.
                        //
                        lCount--;

                        //
                        // See if there is another pixel to draw.
                        //
                        if(lCount)
                        {
                case 1:
                            //
                            // Get the lower nibble of the next byte of pixel
                            // data and extract the corresponding entry from
                            // the palette.
                            //
                            ulByte = (*pucData++ & 15) * 3;
                            ulByte = (*(unsigned long *)(pucPalette + ulByte) &
                                      0x00ffffff);

                            //
                            // Translate this palette entry and write it to the
                            // screen.
                            //
                            WriteData(DPYCOLORTRANSLATE(ulByte));

                            //
                            // Decrement the count of pixels to draw.
                            //
                            lCount--;
                        }
                    }
            }

            //
            // The image data has been drawn.
            //
            break;
        }

        //
        // The pixel data is in 8 bit per pixel format.
        //
        case 8:
        {
            //
            // Loop while there are more pixels to draw.
            //
            while(lCount--)
            {
                //
                // Get the next byte of pixel data and extract the
                // corresponding entry from the palette.
                //
                ulByte = *pucData++ * 3;
                ulByte = *(unsigned long *)(pucPalette + ulByte) & 0x00ffffff;

                //
                // Translate this palette entry and write it to the screen.
                //
                WriteData(DPYCOLORTRANSLATE(ulByte));
            }

            //
            // The image data has been drawn.
            //
            break;
        }

        //
        // We are being passed data in the display's native format.  Merely
        // write it directly to the display.  This is a special case which is
        // not used by the graphics library but which is helpful to
        // applications which may want to handle, for example, JPEG images.
        //
        case 16:
        {
            unsigned short usByte;

            //
            // Loop while there are more pixels to draw.
            //
            while(lCount--)
            {
                //
                // Get the next byte of pixel data and extract the
                // corresponding entry from the palette.
                //
                usByte = *((unsigned short *)pucData);
                pucData += 2;

                //
                // Translate this palette entry and write it to the screen.
                //
                WriteData(usByte);
            }
        }
    }
}

//*****************************************************************************
//
//! Draws a horizontal line.
//!
//! \param pvDisplayData is a pointer to the driver-specific data for this
//! display driver.
//! \param lX1 is the X coordinate of the start of the line.
//! \param lX2 is the X coordinate of the end of the line.
//! \param lY is the Y coordinate of the line.
//! \param ulValue is the color of the line.
//!
//! This function draws a horizontal line on the display.  The coordinates of
//! the line are assumed to be within the extents of the display.
//!
//! \return None.
//
//*****************************************************************************
static void
Kitronix320x240x16_SSD2119LineDrawH(void *pvDisplayData, long lX1, long lX2,
                                   long lY, unsigned long ulValue)
{
    //
    // Set the cursor increment to left to right, followed by top to bottom.
    //
    WriteCommand(SSD2119_ENTRY_MODE_REG);
    WriteData(MAKE_ENTRY_MODE(HORIZ_DIRECTION));

    //
    // Set the starting X address of the display cursor.
    //
    WriteCommand(SSD2119_X_RAM_ADDR_REG);
    WriteData(MAPPED_X(lX1, lY));

    //
    // Set the Y address of the display cursor.
    //
    WriteCommand(SSD2119_Y_RAM_ADDR_REG);
    WriteData(MAPPED_Y(lX1, lY));

    //
    // Write the data RAM write command.
    //
    WriteCommand(SSD2119_RAM_DATA_REG);

    //
    // Loop through the pixels of this horizontal line.
    //
    while(lX1++ <= lX2)
    {
        //
        // Write the pixel value.
        //
        WriteData(ulValue);
    }
}

//*****************************************************************************
//
//! Draws a vertical line.
//!
//! \param pvDisplayData is a pointer to the driver-specific data for this
//! display driver.
//! \param lX is the X coordinate of the line.
//! \param lY1 is the Y coordinate of the start of the line.
//! \param lY2 is the Y coordinate of the end of the line.
//! \param ulValue is the color of the line.
//!
//! This function draws a vertical line on the display.  The coordinates of the
//! line are assumed to be within the extents of the display.
//!
//! \return None.
//
//*****************************************************************************
static void
Kitronix320x240x16_SSD2119LineDrawV(void *pvDisplayData, long lX, long lY1,
                                   long lY2, unsigned long ulValue)
{
    //
    // Set the cursor increment to top to bottom, followed by left to right.
    //
    WriteCommand(SSD2119_ENTRY_MODE_REG);
    WriteData(MAKE_ENTRY_MODE(VERT_DIRECTION));

    //
    // Set the X address of the display cursor.
    //
    WriteCommand(SSD2119_X_RAM_ADDR_REG);
    WriteData(MAPPED_X(lX, lY1));

    //
    // Set the starting Y address of the display cursor.
    //
    WriteCommand(SSD2119_Y_RAM_ADDR_REG);
    WriteData(MAPPED_Y(lX, lY1));

    //
    // Write the data RAM write command.
    //
    WriteCommand(SSD2119_RAM_DATA_REG);

    //
    // Loop through the pixels of this vertical line.
    //
    while(lY1++ <= lY2)
    {
        //
        // Write the pixel value.
        //
        WriteData(ulValue);
    }
}

//*****************************************************************************
//
//! Fills a rectangle.
//!
//! \param pvDisplayData is a pointer to the driver-specific data for this
//! display driver.
//! \param pRect is a pointer to the structure describing the rectangle.
//! \param ulValue is the color of the rectangle.
//!
//! This function fills a rectangle on the display.  The coordinates of the
//! rectangle are assumed to be within the extents of the display, and the
//! rectangle specification is fully inclusive (in other words, both sXMin and
//! sXMax are drawn, along with sYMin and sYMax).
//!
//! \return None.
//
//*****************************************************************************
static void
Kitronix320x240x16_SSD2119RectFill(void *pvDisplayData, const tRectangle *pRect,
                                  unsigned long ulValue)
{
    long lCount;

    //
    // Write the Y extents of the rectangle.
    //
    WriteCommand(SSD2119_ENTRY_MODE_REG);
    WriteData(MAKE_ENTRY_MODE(HORIZ_DIRECTION));

    //
    // Write the X extents of the rectangle.
    //
    WriteCommand(SSD2119_H_RAM_START_REG);
#if (defined PORTRAIT) || (defined LANDSCAPE)
    WriteData(MAPPED_X(pRect->sXMax, pRect->sYMax));
#else
    WriteData(MAPPED_X(pRect->sXMin, pRect->sYMin));
#endif

    WriteCommand(SSD2119_H_RAM_END_REG);
#if (defined PORTRAIT) || (defined LANDSCAPE)
    WriteData(MAPPED_X(pRect->sXMin, pRect->sYMin));
#else
    WriteData(MAPPED_X(pRect->sXMax, pRect->sYMax));
#endif

    //
    // Write the Y extents of the rectangle
    //
    WriteCommand(SSD2119_V_RAM_POS_REG);
#if (defined LANDSCAPE_FLIP) || (defined PORTRAIT)
    WriteData(MAPPED_Y(pRect->sXMin, pRect->sYMin) |
             (MAPPED_Y(pRect->sXMax, pRect->sYMax) << 8));
#else
    WriteData(MAPPED_Y(pRect->sXMax, pRect->sYMax) |
             (MAPPED_Y(pRect->sXMin, pRect->sYMin) << 8));
#endif

    //
    // Set the display cursor to the upper left of the rectangle (in application
    // coordinate space).
    //
    WriteCommand(SSD2119_X_RAM_ADDR_REG);
    WriteData(MAPPED_X(pRect->sXMin, pRect->sYMin));

    WriteCommand(SSD2119_Y_RAM_ADDR_REG);
    WriteData(MAPPED_Y(pRect->sXMin, pRect->sYMin));

    //
    // Tell the controller we are about to write data into its RAM.
    //
    WriteCommand(SSD2119_RAM_DATA_REG);

    //
    // Loop through the pixels of this filled rectangle.
    //
    for(lCount = ((pRect->sXMax - pRect->sXMin + 1) *
                  (pRect->sYMax - pRect->sYMin + 1)); lCount >= 0; lCount--)
    {
        //
        // Write the pixel value.
        //
        WriteData(ulValue);
    }

    //
    // Reset the X extents to the entire screen.
    //
    WriteCommand(SSD2119_H_RAM_START_REG);
    WriteData(0x0000);
    WriteCommand(SSD2119_H_RAM_END_REG);
    WriteData(0x013F);

    //
    // Reset the Y extent to the full screen
    //
    WriteCommand(SSD2119_V_RAM_POS_REG);
    WriteData(0xEF00);
}

//*****************************************************************************
//
//! Translates a 24-bit RGB color to a display driver-specific color.
//!
//! \param pvDisplayData is a pointer to the driver-specific data for this
//! display driver.
//! \param ulValue is the 24-bit RGB color.  The least-significant byte is the
//! blue channel, the next byte is the green channel, and the third byte is the
//! red channel.
//!
//! This function translates a 24-bit RGB color into a value that can be
//! written into the display's frame buffer in order to reproduce that color,
//! or the closest possible approximation of that color.
//!
//! \return Returns the display-driver specific color.
//
//*****************************************************************************
static unsigned long
Kitronix320x240x16_SSD2119ColorTranslate(void *pvDisplayData,
                                        unsigned long ulValue)
{
    //
    // Translate from a 24-bit RGB color to a 5-6-5 RGB color.
    //
    return(DPYCOLORTRANSLATE(ulValue));
}

//*****************************************************************************
//
//! Flushes any cached drawing operations.
//!
//! \param pvDisplayData is a pointer to the driver-specific data for this
//! display driver.
//!
//! This functions flushes any cached drawing operations to the display.  This
//! is useful when a local frame buffer is used for drawing operations, and the
//! flush would copy the local frame buffer to the display.  For the SSD2119
//! driver, the flush is a no operation.
//!
//! \return None.
//
//*****************************************************************************
static void
Kitronix320x240x16_SSD2119Flush(void *pvDisplayData)
{
    //
    // There is nothing to be done.
    //
}

//*****************************************************************************
//
//! The display structure that describes the driver for the Kitronix
//! K350QVG-V1-F TFT panel with an SSD2119 controller.
//
//*****************************************************************************
const tDisplay g_sKitronix320x240x16_SSD2119 =
{
    sizeof(tDisplay),
    0,
#if defined(PORTRAIT) || defined(PORTRAIT_FLIP)
    240,
    320,
#else
    320,
    240,
#endif
    Kitronix320x240x16_SSD2119PixelDraw,
    Kitronix320x240x16_SSD2119PixelDrawMultiple,
    Kitronix320x240x16_SSD2119LineDrawH,
    Kitronix320x240x16_SSD2119LineDrawV,
    Kitronix320x240x16_SSD2119RectFill,
    Kitronix320x240x16_SSD2119ColorTranslate,
    Kitronix320x240x16_SSD2119Flush
};

//*****************************************************************************
//
// Close the Doxygen group.
//! @}
//
//*****************************************************************************
