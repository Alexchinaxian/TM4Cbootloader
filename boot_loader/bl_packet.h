//*****************************************************************************
//
// bl_packet.h - The global variables and definitions of the boot loader.
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

#ifndef __BL_PACKET_H__
#define __BL_PACKET_H__
#define UART0_BASE 0x4000C000

typedef struct {
    union CRC
    {
        uint16_t CRC;
        struct
        {
         uint8_t crc_L;
         uint8_t crc_H;
        };
    }CRC;
    union Address
    {
        uint16_t Address;
        struct
        {
         uint8_t addressL;
         uint8_t addressH;
        };
    }ADDRESS;
    uint8_t packetData[128];
    uint8_t Num[2];
    uint8_t CMD;
    uint8_t ID;
} Receive_Package ;


typedef struct {
    uint8_t ID;
    uint8_t CMD;
    union BMS_Address
    {
        uint16_t Address;
        struct
        {
            uint8_t addressH;
            uint8_t addressL;
        };
    }ADDRESS;
    uint8_t packetData[2];
    union BMS_CRC
    {
        uint16_t CRC;
        struct
        {
            uint8_t crc_L;
            uint8_t crc_H;
        };
    }CRC;
} Send_Package;
Send_Package txbuff;
Receive_Package rxbuff;

//*****************************************************************************
//
// Packet Handling APIs
//
//*****************************************************************************
extern int ReceivePacket(Receive_Package *packet);
extern int SendPacket(uint8_t *pui8Data, uint32_t ui32Size);
extern void AckPacket(void);

#endif // __BL_PACKET_H__
