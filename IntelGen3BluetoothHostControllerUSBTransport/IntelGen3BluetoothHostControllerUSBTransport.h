/*
 *  Released under "The GNU General Public License (GPL-2.0)"
 *
 *  Copyright (c) 2021 cjiang. All rights reserved.
 *  Copyright (C) 2015 Intel Corporation.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 *  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#ifndef IntelGen3BluetoothHostController_h
#define IntelGen3BluetoothHostController_h

#include "../IntelBluetoothHostControllerUSBTransport/IntelBluetoothHostControllerUSBTransport.h"

class IntelGen3BluetoothHostControllerUSBTransport : public IntelBluetoothHostControllerUSBTransport
{
    OSDeclareDefaultStructors(IntelGen3BluetoothHostControllerUSBTransport)
    
public:
    virtual bool start(IOService * provider) APPLE_KEXT_OVERRIDE;
    
    virtual IOReturn ParseVersionInfoTLV(BluetoothIntelVersionInfoTLV * version, UInt8 * data, IOByteCount dataSize);
    
    virtual IOReturn GetFirmwareNameWL(void * version, BluetoothIntelBootParams * params, const char * suffix, char * fwName) APPLE_KEXT_OVERRIDE;
    virtual IOReturn GetFirmwareWL(void * version, BluetoothIntelBootParams * params, const char * suffix, OSData ** fwData) APPLE_KEXT_OVERRIDE;
    virtual IOReturn DownloadFirmwareWL(BluetoothHCIRequestID inID, void * version, BluetoothIntelBootParams * params, UInt32 * bootAddress) APPLE_KEXT_OVERRIDE;
    
    OSMetaClassDeclareReservedUnused(IntelGen3BluetoothHostControllerUSBTransport, 0);
    OSMetaClassDeclareReservedUnused(IntelGen3BluetoothHostControllerUSBTransport, 1);
    OSMetaClassDeclareReservedUnused(IntelGen3BluetoothHostControllerUSBTransport, 2);
    OSMetaClassDeclareReservedUnused(IntelGen3BluetoothHostControllerUSBTransport, 3);
    OSMetaClassDeclareReservedUnused(IntelGen3BluetoothHostControllerUSBTransport, 4);
    OSMetaClassDeclareReservedUnused(IntelGen3BluetoothHostControllerUSBTransport, 5);
    OSMetaClassDeclareReservedUnused(IntelGen3BluetoothHostControllerUSBTransport, 6);
    OSMetaClassDeclareReservedUnused(IntelGen3BluetoothHostControllerUSBTransport, 7);
    OSMetaClassDeclareReservedUnused(IntelGen3BluetoothHostControllerUSBTransport, 8);
    OSMetaClassDeclareReservedUnused(IntelGen3BluetoothHostControllerUSBTransport, 9);
    OSMetaClassDeclareReservedUnused(IntelGen3BluetoothHostControllerUSBTransport, 10);
    OSMetaClassDeclareReservedUnused(IntelGen3BluetoothHostControllerUSBTransport, 11);
    OSMetaClassDeclareReservedUnused(IntelGen3BluetoothHostControllerUSBTransport, 12);
    OSMetaClassDeclareReservedUnused(IntelGen3BluetoothHostControllerUSBTransport, 13);
    OSMetaClassDeclareReservedUnused(IntelGen3BluetoothHostControllerUSBTransport, 14);
    OSMetaClassDeclareReservedUnused(IntelGen3BluetoothHostControllerUSBTransport, 15);
    OSMetaClassDeclareReservedUnused(IntelGen3BluetoothHostControllerUSBTransport, 16);
    OSMetaClassDeclareReservedUnused(IntelGen3BluetoothHostControllerUSBTransport, 17);
    OSMetaClassDeclareReservedUnused(IntelGen3BluetoothHostControllerUSBTransport, 18);
    OSMetaClassDeclareReservedUnused(IntelGen3BluetoothHostControllerUSBTransport, 19);
    OSMetaClassDeclareReservedUnused(IntelGen3BluetoothHostControllerUSBTransport, 20);
    OSMetaClassDeclareReservedUnused(IntelGen3BluetoothHostControllerUSBTransport, 21);
    OSMetaClassDeclareReservedUnused(IntelGen3BluetoothHostControllerUSBTransport, 22);
    OSMetaClassDeclareReservedUnused(IntelGen3BluetoothHostControllerUSBTransport, 23);
};

#endif
