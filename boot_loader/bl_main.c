//*****************************************************************************
//
// bl_main.c - The file holds the main control loop of the boot loader.
//
// Copyright (c) 2006-2020 Texas Instruments Incorporated.  All rights reserved.
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
// This is part of revision 2.2.0.295 of the Tiva Firmware Development Package.
//
//*****************************************************************************

#include <stdint.h>
#include <stdbool.h>
#include "inc/hw_gpio.h"
#include "inc/hw_flash.h"
#include "inc/hw_i2c.h"
#include "inc/hw_memmap.h"
#include "inc/hw_nvic.h"
#include "inc/hw_ssi.h"
#include "inc/hw_sysctl.h"
#include "inc/hw_types.h"
#include "inc/hw_uart.h"
#include "bl_config.h"
#include "driverlib/sysctl.h"
#include "driverlib/gpio.h"
#include "driverlib/pin_map.h"
#include "driverlib/uart.h"
#include "driverlib/interrupt.h"
#include "boot_loader/bl_commands.h"
#include "boot_loader/bl_decrypt.h"
#include "boot_loader/bl_flash.h"
#include "boot_loader/bl_hooks.h"
#include "boot_loader/bl_i2c.h"
#include "boot_loader/bl_packet.h"
#include "boot_loader/bl_ssi.h"
#include "boot_loader/bl_uart.h"
#include "driverlib/flash.h"

#ifdef CHECK_CRC
#include "boot_loader/bl_crc32.h"
#endif
extern void BOOTRun(uint32_t BaseAddr);
//*****************************************************************************
//
// Make sure that the application start address falls on a flash page boundary
//
//*****************************************************************************
#if (APP_START_ADDRESS & (FLASH_PAGE_SIZE - 1))
#error ERROR: APP_START_ADDRESS must be a multiple of FLASH_PAGE_SIZE bytes!
#endif

//*****************************************************************************
//
// Make sure that the flash reserved space is a multiple of flash pages.
//
//*****************************************************************************
#if (FLASH_RSVD_SPACE & (FLASH_PAGE_SIZE - 1))
#error ERROR: FLASH_RSVD_SPACE must be a multiple of FLASH_PAGE_SIZE bytes!
#endif

//*****************************************************************************
//
//! \addtogroup bl_main_api
//! @{
//
//*****************************************************************************
#if defined(I2C_ENABLE_UPDATE) || defined(SSI_ENABLE_UPDATE) || \
    defined(UART_ENABLE_UPDATE) || defined(DOXYGEN)

//*****************************************************************************
//
// A prototype for the function (in the startup code) for calling the
// application.
//
//*****************************************************************************
extern void CallApplication(uint32_t);

//*****************************************************************************
//
// A prototype for the function (in the startup code) for a predictable length
// delay.
//
//*****************************************************************************
extern void Delay(uint32_t ui32Count);

//*****************************************************************************
//
// Holds the current status of the last command that was issued to the boot
// loader.
//
//*****************************************************************************
uint8_t g_ui8Status;

//*****************************************************************************
//
// This holds the current remaining size in bytes to be downloaded.
//
//*****************************************************************************
uint32_t g_ui32TransferSize;

//*****************************************************************************
//
// This holds the total size of the firmware image being downloaded (if the
// protocol in use provides this).
//
//*****************************************************************************
#if (defined BL_PROGRESS_FN_HOOK) || (defined CHECK_CRC)
uint32_t g_ui32ImageSize;
#endif

//*****************************************************************************
//
// This holds the current address that is being written to during a download
// command.
//
//*****************************************************************************
uint32_t g_ui32TransferAddress;
#ifdef CHECK_CRC
uint32_t g_ui32ImageAddress;
#endif

//*****************************************************************************
//
// This is the data buffer used during transfers to the boot loader.
//
//*****************************************************************************
uint32_t g_pui32DataBuffer[BUFFER_SIZE];

//*****************************************************************************
//
// This is an specially aligned buffer pointer to g_pui32DataBuffer to make
// copying to the buffer simpler.  It must be offset to end on an address that
// ends with 3.
//
//*****************************************************************************
uint8_t *g_pui8DataBuffer;

//*****************************************************************************
//
// Converts a word from big endian to little endian.  This macro uses compiler-
// specific constructs to perform an inline insertion of the "rev" instruction,
// which performs the byte swap directly.
//
//*****************************************************************************
#if defined(ewarm)
#include <intrinsics.h>
#define SwapWord(x)             __REV(x)
#endif
#if defined(codered) || defined(gcc) || defined(sourcerygxx)
#define SwapWord(x) __extension__                                             \
        ({                                                                    \
             register uint32_t __ret, __inp = x;                              \
             __asm__("rev %0, %1" : "=r" (__ret) : "r" (__inp));              \
             __ret;                                                           \
        })
#endif
#if defined(rvmdk) || defined(__ARMCC_VERSION)
#define SwapWord(x)             __rev(x)
#endif
#if defined(ccs)
uint32_t
SwapWord(uint32_t x)
{
    __asm("    rev     r0, r0\n"
          "    bx      lr\n"); // need this to make sure r0 is returned
    return(x + 1); // return makes compiler happy - ignored
}
#endif
//*****************************************************************************
//
//! This function performs the update on the selected port.
//!
//! This function is called directly by the boot loader or it is called as a
//! result of an update request from the application.
//!
//! \return Never returns.
//
//*****************************************************************************
//#define
union
{
    uint32_t g_pui32DataBuffer;
    union
    {
        uint16_t u16ADD_H;
        struct
        {
            uint8_t add_H;
            uint8_t add_L;
        };
    }ADD_H;
    union
    {
        uint16_t u16ADD_L;
        struct
        {
            uint8_t add_H;
            uint8_t add_L;
        };
    }ADD_L;
}Program_Address;
union
{
    uint32_t g_pui32DataSize;
    struct
    {
        union
        {
            uint16_t u16size_H;
            struct
            {
                uint8_t size_H;
                uint8_t size_L;
            };
        }Size_H;
        union
        {
            uint16_t u16size_L;
            struct
            {
                uint8_t size_H;
                uint8_t size_L;
            };
        }Size_L;
    };
}Program_Size;

union FlashBuff
    {
        uint8_t flash_buff[128];
        struct flash
        {
         uint8_t buff_1;
         uint8_t buff_2;
         uint8_t buff_3;
         uint8_t buff_4;
        }flashbuff[32];
    }FlashBuff;

#define Handshake  0x6001        //握手
#define WriteInfo  0x6003    //设置多字节写数据相关参数（写起始地址，数据量）
#define Write = 0x6006        //以多字节形式写数据
#define ExitBootloader  0x6000 //退出boot

int i;

void ConfigureDevice(void){
    uint32_t g_ui32SysClock;
    g_ui32SysClock = SysCtlClockFreqSet((SYSCTL_XTAL_16MHZ |
                                             SYSCTL_OSC_MAIN |
                                             SYSCTL_USE_PLL |
                                         SYSCTL_CFG_VCO_480),80000000);
            SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
            SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);//uart0对应的外设引脚为PA0，PA1
            //设置PA0，PA1为uart引脚
            GPIOPinConfigure(GPIO_PA0_U0RX);
            GPIOPinConfigure(GPIO_PA1_U0TX);
            GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);
            //设置波特率115200，数据位 8 ，校验位 None ，停止位 1 ，8——N——1模式
            UARTConfigSetExpClk(UART0_BASE, g_ui32SysClock, 115200,
                                    (UART_CONFIG_WLEN_8 |  UART_CONFIG_STOP_ONE |
                                     UART_CONFIG_PAR_NONE));
            UARTIntEnable(UART0_BASE, UART_INT_RX | UART_INT_RT);
            SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
            GPIOPinTypeGPIOOutput(GPIO_PORTA_BASE, GPIO_PIN_2);
}
uint8_t g_pui8ACK[2] = { 0, COMMAND_ACK };
void Updater(void)
{
    uint32_t ui32Temp, ui32FlashSize;
    //
    // Insure that the COMMAND_SEND_DATA cannot be sent to erase the boot
    // loader before the application is erased.
    //
    g_ui32TransferAddress = 0xffffffff;

    //
    // Read any data from the serial port in use.
    //
    while(1)
    {
        //
        // Receive a packet from the port in use.
        //
        ReceivePacket(&rxbuff);
        //
        // The first byte of the data buffer has the command and determines
        // the format of the rest of the bytes.
        //
        switch(rxbuff.ADDRESS.Address)
        {
            //
            // This was a simple ping command.
            case Handshake:
            {
                //
                // This command always sets the status to COMMAND_RET_SUCCESS.
                //
                g_ui8Status = COMMAND_RET_SUCCESS;

                //
                // Just acknowledge that the command was received.
                //
                txbuff.ID = 0x21;
                txbuff.CMD = 0x02;
                txbuff.ADDRESS.Address = 0x0260;
                txbuff.packetData[0] = 0x00;
                txbuff.packetData[1] = COMMAND_ACK;
                txbuff.CRC.CRC = 0xCC00;
                SendData(&txbuff, 8);
                break;
            }

            //
            // This command indicates the start of a download sequence.
            case WriteInfo:
            {
                if(rxbuff.CMD == 0x10){
                    // Until determined otherwise, the command status is success.
                    g_ui8Status = COMMAND_RET_SUCCESS;
                    Program_Address.ADD_L.add_H = rxbuff.packetData[2];
                    Program_Address.ADD_L.add_L = rxbuff.packetData[3];
                    Program_Address.ADD_H.add_H = rxbuff.packetData[4];
                    Program_Address.ADD_H.add_L = rxbuff.packetData[5];
                    Program_Size.Size_L.size_H = rxbuff.packetData[6];
                    Program_Size.Size_L.size_L = rxbuff.packetData[7];
                    Program_Size.Size_H.size_H = rxbuff.packetData[8];
                    Program_Size.Size_H.size_L = rxbuff.packetData[9];

                    g_ui32TransferAddress = Program_Address.g_pui32DataBuffer;
                    g_ui32TransferSize = Program_Size.g_pui32DataSize;
                    // Check for a valid starting address and image size.
                    if(!BL_FLASH_AD_CHECK_FN_HOOK(g_ui32TransferAddress,
                                                  g_ui32TransferSize))
                    {
                        // Set the code to an error to indicate that the last
                        // command failed.  This informs the updater program
                        // that the download command failed.
                        g_ui8Status = COMMAND_RET_INVALID_ADR;

                        // This packet has been handled.
                        break;
                    }
                    ui32FlashSize = g_ui32TransferAddress + g_ui32TransferSize;
                    // Clear the flash access interrupt.
                    BL_FLASH_CL_ERR_FN_HOOK();
                        // Leave the boot loader present until we start getting an
                        // image.
                    for(ui32Temp = g_ui32TransferAddress;
                        ui32Temp < ui32FlashSize; ui32Temp += FLASH_PAGE_SIZE)
                    {
                        // Erase this block.
                        BL_FLASH_ERASE_FN_HOOK(ui32Temp);
                    }
                    // Return an error if an access violation occurred.
                    if(BL_FLASH_ERROR_FN_HOOK())
                    {
                        g_ui8Status = COMMAND_RET_FLASH_FAIL;
                    }

                    // See if the command was successful.
                    if(g_ui8Status != COMMAND_RET_SUCCESS)
                    {
                        // Setting g_ui32TransferSize to zero makes
                        // COMMAND_SEND_DATA fail to accept any data
                        g_ui32TransferSize = 0;
                    }
                    // Acknowledge that this command was received correctly.  This
                    // does not indicate success, just that the command was
                    // received.
                    AckPacket();

                    // Go back and wait for a new command.
                    break;
                }
                else if(rxbuff.CMD == 0x03){
                    // Acknowledge that this command was received correctly.  This
                    // does not indicate success, just that the command was
                    // received.
                    txbuff.ID = 0x01;
                    txbuff.CMD = 0x03;
                    txbuff.ADDRESS.Address = 0x0600;
                    txbuff.packetData[0] = Program_Address.ADD_L.add_L;
                    txbuff.packetData[1] = Program_Address.ADD_L.add_H;
                    txbuff.CRC.CRC = 0x0100;
                    SendData(&txbuff, 8);
                    // Go back and wait for a new command.
                    break;
                }
            }

            //
            // This command indicates that control should be transferred to
            // the specified address.
            //
            case COMMAND_RUN:
            {
                //
                // Acknowledge that this command was received correctly.  This
                // does not indicate success, just that the command was
                // received.
                //
                AckPacket();
                //
                // Get the address to which control should be transferred.
                //
                g_ui32TransferAddress = SwapWord(g_pui32DataBuffer[1]);

                //
                // This determines the size of the flash available on the
                // device in use.
                //
                ui32FlashSize = BL_FLASH_SIZE_FN_HOOK();

                //
                // Test if the transfer address is valid for this device.
                //
                if(g_ui32TransferAddress >= ui32FlashSize)
                {
                    //
                    // Indicate that an invalid address was specified.
                    //
                    g_ui8Status = COMMAND_RET_INVALID_ADR;

                    //
                    // This packet has been handled.
                    //
                    break;
                }

                //
                // Make sure that the ACK packet has been sent.
                //
                FlushData();

                //
                // Reset and disable the peripherals used by the boot loader.
                //
#ifdef I2C_ENABLE_UPDATE
                HWREG(SYSCTL_RCGCI2C) &= ~I2C_CLOCK_ENABLE;
                HWREG(SYSCTL_SRI2C) = I2C_CLOCK_ENABLE;
                HWREG(SYSCTL_SRI2C) = 0;
#endif
#ifdef UART_ENABLE_UPDATE
                HWREG(SYSCTL_RCGCUART) &= ~UART_CLOCK_ENABLE;
                HWREG(SYSCTL_SRUART) = UART_CLOCK_ENABLE;
                HWREG(SYSCTL_SRUART) = 0;
#endif
#ifdef SSI_ENABLE_UPDATE
                HWREG(SYSCTL_RCGCSSI) &= ~SSI_CLOCK_ENABLE;
                HWREG(SYSCTL_SRSSI) = SSI_CLOCK_ENABLE;
                HWREG(SYSCTL_SRSSI) = 0;
#endif

                //
                // Branch to the specified address.  This should never return.
                // If it does, very bad things will likely happen since it is
                // likely that the copy of the boot loader in SRAM will have
                // been overwritten.
                //

                ((void (*)(void))g_ui32TransferAddress)();

                //
                // In case this ever does return and the boot loader is still
                // intact, simply reset the device.
                //
                HWREG(NVIC_APINT) = (NVIC_APINT_VECTKEY |
                                     NVIC_APINT_SYSRESETREQ);

                //
                // The microcontroller should have reset, so this should
                // never be reached.  Just in case, loop forever.
                //
                while(1)
                {
                }
            }

            //
            // This command is sent to transfer data to the device following
            // a download command.
            //
            case WriteBin:
            {
                // Until determined otherwise, the command status is success.
                //
                g_ui8Status = COMMAND_RET_SUCCESS;

                //
                // If this is overwriting the boot loader then the application
                // has already been erased so now erase the boot loader.
                //
                if(g_ui32TransferAddress == 0)
                {
                    //
                    // Clear the flash access interrupt.
                    //
                    BL_FLASH_CL_ERR_FN_HOOK();

                    //
                    // Erase the boot loader.
                    //
                    for(ui32Temp = 0; ui32Temp < APP_START_ADDRESS;
                        ui32Temp += FLASH_PAGE_SIZE)
                    {
                        //
                        // Erase this block.
                        //
                        BL_FLASH_ERASE_FN_HOOK(ui32Temp);
                    }

                    //
                    // Return an error if an access violation occurred.
                    //
                    if(BL_FLASH_ERROR_FN_HOOK())
                    {
                        //
                        // Setting g_ui32TransferSize to zero makes
                        // COMMAND_SEND_DATA fail to accept any more data.
                        //
                        g_ui32TransferSize = 0;

                        //
                        // Indicate that the flash erase failed.
                        //
                        g_ui8Status = COMMAND_RET_FLASH_FAIL;
                    }
                }

                BL_FLASH_PROGRAM_FN_HOOK(g_ui32TransferAddress,
                                         (uint8_t *) &rxbuff.packetData[0],
                                         128);
                //
                // Return an error if an access violation occurred.
                //
                if(BL_FLASH_ERROR_FN_HOOK())
                {
                    //
                    // Indicate that the flash programming failed.
                    //
                    g_ui8Status = COMMAND_RET_FLASH_FAIL;
                }
                else
                {
                    //
                    // Now update the address to program.
                    //
                    g_ui32TransferSize -= 128;
                    g_ui32TransferAddress += 128;

                }
                AckPacket();
                break;
            }

            //
            // This command is used to reset the device.
            //
            case ExitBootloader:
            {
                txbuff.ID = 0x21;
                txbuff.CMD = 0x01;
                txbuff.ADDRESS.addressL = 0x60;
                txbuff.ADDRESS.addressH = 0x02;
                txbuff.packetData[0] = 0x00;
                txbuff.packetData[1] = 0x0B;
                txbuff.CRC.crc_L = 0x0B;
                txbuff.CRC.crc_H= 0X0B;

                SendData(&txbuff, 8);
                //
                // Make sure that the ACK packet has been sent.
                //
                FlushData();
                //
                // Perform a software reset request.  This will cause the
                // microcontroller to reset; no further code will be executed.
                //
                HWREG(NVIC_APINT) = (NVIC_APINT_VECTKEY |
                                     NVIC_APINT_SYSRESETREQ);
                //
                // The microcontroller should have reset, so this should never
                // be reached.  Just in case, loop forever.
                //
                while(1)
                {
                }
            }

            //
            // Just acknowledge the command and set the error to indicate that
            // a bad command was sent.
            //
            default:
            {
                //
                // Acknowledge that this command was received correctly.  This
                // does not indicate success, just that the command was
                // received.
                //
                AckPacket();

                //
                // Indicate that a bad comand was sent.
                //
                g_ui8Status = COMMAND_RET_UNKNOWN_CMD;

                //
                // Go back and wait for a new command.
                //
                break;
            }
        }
    }
}
//*****************************************************************************
#endif
