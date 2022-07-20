//*****************************************************************************
//
// bl_packet.c - Packet handler functions used by the boot loader.
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
#include "bl_config.h"
#include "boot_loader/bl_commands.h"
#include "boot_loader/bl_i2c.h"
#include "boot_loader/bl_packet.h"
#include "boot_loader/bl_ssi.h"
#include "boot_loader/bl_uart.h"

//*****************************************************************************
//
//! \addtogroup bl_packet_api
//! @{
//
//*****************************************************************************
#if defined(I2C_ENABLE_UPDATE) || defined(SSI_ENABLE_UPDATE) || \
    defined(UART_ENABLE_UPDATE) || defined(DOXYGEN)

//*****************************************************************************
//
// The packet that is sent to acknowledge a received packet.
//
//*****************************************************************************
static const uint8_t g_pui8ACK[2] = { 0, COMMAND_ACK };

//*****************************************************************************
//
// The packet that is sent to not-acknowledge a received packet.
//
//*****************************************************************************
static const uint8_t g_pui8NAK[2] = { 0, COMMAND_NAK };

//*****************************************************************************
//
//! Calculates an 8-bit checksum
//!
//! \param pui8Data is a pointer to an array of 8-bit data of size ui32Size.
//! \param ui32Size is the size of the array that will run through the checksum
//! algorithm.
//!
//! This function simply calculates an 8-bit checksum on the data passed in.
//!
//! \return Returns the calculated checksum.
//
//*****************************************************************************
uint32_t
CheckSum(const uint8_t *pui8Data, uint32_t ui32Size)
{
    uint32_t ui32CheckSum;

    //
    // Initialize the checksum to zero.
    //
    ui32CheckSum = 0;

    //
    // Add up all the bytes, do not do anything for an overflow.
    //
    while(ui32Size--)
    {
        ui32CheckSum += *pui8Data++;
    }

    //
    // Return the caculated check sum.
    //
    return(ui32CheckSum & 0xff);
}

//*****************************************************************************
//
//! Sends an Acknowledge packet.
//!
//! This function is called to acknowledge that a packet has been received by
//! the microcontroller.
//!
//! \return None.
//
//*****************************************************************************
void
AckPacket(void)
{
    //
    // ACK/NAK packets are the only ones with no size.
    //
    SendData(g_pui8ACK, 2);
}

//*****************************************************************************
//
//! Sends a no-acknowledge packet.
//!
//! This function is called when an invalid packet has been received by the
//! microcontroller, indicating that it should be retransmitted.
//!
//! \return None.
//
//*****************************************************************************
void
NakPacket(void)
{
    //
    // ACK/NAK packets are the only ones with no size.
    //
    SendData(g_pui8NAK, 2);
}

//*****************************************************************************
//
//! Receives a data packet.
//!
//! \param pui8Data is the location to store the data that is sent to the boot
//! loader.
//! \param pui32Size is the number of bytes returned in the pui8Data buffer
//! that was provided.
//!
//! This function receives a packet of data from specified transfer function.
//!
//! \return Returns zero to indicate success while any non-zero value indicates
//! a failure.
//
//*****************************************************************************
int ReceivePacket(Receive_Package *packet)
{
    int index,num;
    UARTReceive(&rxbuff.ID,1);
    UARTReceive(&rxbuff.CMD, 1);
    UARTReceive(&rxbuff.ADDRESS.addressH, 1);
    UARTReceive(&rxbuff.ADDRESS.addressL, 1);
    if(rxbuff.ADDRESS.Address == 0x6000){
        if(rxbuff.CMD == 0x06){
            UARTReceive(&rxbuff.packetData[0], 1);
            UARTReceive(&rxbuff.packetData[2], 1);
        }
        else if(rxbuff.CMD == 0x10){
            for (index = 0;index < 3; index++) {
                UARTReceive(&rxbuff.packetData[index], 1);
            }
        }
        else{
         //None for 0x6004
        }
    }
    else if(rxbuff.ADDRESS.Address == 0x6001){
        UARTReceive(&rxbuff.packetData[0], 1);
        UARTReceive(&rxbuff.packetData[1], 1);
    }
    else if(rxbuff.ADDRESS.Address == 0x6002){
        for (index = 0;index < 3; index++) {
            UARTReceive(&rxbuff.packetData[index], 1);
        }
    }
    else if(rxbuff.ADDRESS.Address == 0x6003){
        if(rxbuff.CMD == 0x10){
            for(index = 0;index < 10; index++){
            UARTReceive(&rxbuff.packetData[index], 1);
            }
        }
        else if(rxbuff.CMD == 0x03){
            UARTReceive(&rxbuff.packetData[0], 1);
            UARTReceive(&rxbuff.packetData[1], 1);
        }
        else{
            //none
        }
    }
    else if(rxbuff.ADDRESS.Address == 0x6006){
        if(rxbuff.CMD == 0x10){
             num = 128;
             for (index = 0; index < num; index++) {
                 UARTReceive(&rxbuff.packetData[index], 1);
             }
         }
        else{
            //none
        }
    }
    else{
        //none for 0x6000
    }
    rxbuff.CRC.crc_H= UARTCharGet(UART0_BASE);
    rxbuff.CRC.crc_L= UARTCharGet(UART0_BASE);


    return(0);
}
//*****************************************************************************
//
//! Sends a data packet.
//!
//! \param pui8Data is the location of the data to be sent.
//! \param ui32Size is the number of bytes to send.
//!
//! This function sends the data provided in the \e pui8Data parameter in the
//! packet format used by the boot loader.  The caller only needs to specify
//! the buffer with the data that needs to be transferred.  This function
//! addresses all other packet formatting issues.
//!
//! \return Returns zero to indicate success while any non-zero value indicates
//! a failure.
//
//*****************************************************************************
int
SendPacket(uint8_t *pui8Data, uint32_t ui32Size)
{
    uint32_t ui32Temp;

    //
    // Caculate the checksum to be sent out with the data.
    //
    ui32Temp = CheckSum(pui8Data, ui32Size);

    //
    // Need to include the size and checksum bytes in the packet.
    //
    ui32Size += 2;

    //
    // Send out the size followed by the data.
    //
    SendData((uint8_t *)&ui32Size, 1);
    SendData((uint8_t *)&ui32Temp, 1);
    SendData(pui8Data, ui32Size - 2);

    //
    // Wait for a non zero byte.
    //
    ui32Temp = 0;
    while(ui32Temp == 0)
    {
        ReceiveData((uint8_t *)&ui32Temp, 1);
    }

    //
    // Check if the byte was a valid ACK and return a negative value if it was
    // not and aknowledge.
    //
    if(ui32Temp != COMMAND_ACK)
    {
        return(-1);
    }

    //
    // This packet was sent and received successfully.
    //
    return(0);
}

//*****************************************************************************
//
// Close the Doxygen group.
//! @}
//
//*****************************************************************************
#endif
