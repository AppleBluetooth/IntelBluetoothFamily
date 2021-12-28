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

#ifndef IntelGen1BluetoothHostControllerUSBTransport_h
#define IntelGen1BluetoothHostControllerUSBTransport_h

#include "../USB/IntelBluetoothHostControllerUSBTransport.h"
#include <FirmwareList.h>

class IntelGen1BluetoothHostControllerUSBTransport : public IntelBluetoothHostControllerUSBTransport
{
    OSDeclareDefaultStructors(IntelGen1BluetoothHostControllerUSBTransport)

public:
    virtual bool start(IOService * provider) APPLE_KEXT_OVERRIDE;

    virtual void ReceiveInterruptData(void * data, UInt32 dataSize, bool special) APPLE_KEXT_OVERRIDE;
    
    virtual IOReturn GetFirmwareNameWL(void * version, BluetoothIntelBootParams * params, const char * suffix, char * fwName) APPLE_KEXT_OVERRIDE;
    virtual IOReturn GetFirmwareErrorHandler(void * version, BluetoothIntelBootParams * params, const char * suffix, OSData ** fwData) APPLE_KEXT_OVERRIDE;
    virtual IOReturn PatchFirmware(OSData * fwData, UInt8 ** fwPtr, int * disablePatch) APPLE_KEXT_OVERRIDE;

    OSMetaClassDeclareReservedUnused(IntelGen1BluetoothHostControllerUSBTransport, 0);
    OSMetaClassDeclareReservedUnused(IntelGen1BluetoothHostControllerUSBTransport, 1);
    OSMetaClassDeclareReservedUnused(IntelGen1BluetoothHostControllerUSBTransport, 2);
    OSMetaClassDeclareReservedUnused(IntelGen1BluetoothHostControllerUSBTransport, 3);
    OSMetaClassDeclareReservedUnused(IntelGen1BluetoothHostControllerUSBTransport, 4);
    OSMetaClassDeclareReservedUnused(IntelGen1BluetoothHostControllerUSBTransport, 5);
    OSMetaClassDeclareReservedUnused(IntelGen1BluetoothHostControllerUSBTransport, 6);
    OSMetaClassDeclareReservedUnused(IntelGen1BluetoothHostControllerUSBTransport, 7);
    OSMetaClassDeclareReservedUnused(IntelGen1BluetoothHostControllerUSBTransport, 8);
    OSMetaClassDeclareReservedUnused(IntelGen1BluetoothHostControllerUSBTransport, 9);
    OSMetaClassDeclareReservedUnused(IntelGen1BluetoothHostControllerUSBTransport, 10);
    OSMetaClassDeclareReservedUnused(IntelGen1BluetoothHostControllerUSBTransport, 11);
    OSMetaClassDeclareReservedUnused(IntelGen1BluetoothHostControllerUSBTransport, 12);
    OSMetaClassDeclareReservedUnused(IntelGen1BluetoothHostControllerUSBTransport, 13);
    OSMetaClassDeclareReservedUnused(IntelGen1BluetoothHostControllerUSBTransport, 14);
    OSMetaClassDeclareReservedUnused(IntelGen1BluetoothHostControllerUSBTransport, 15);
    OSMetaClassDeclareReservedUnused(IntelGen1BluetoothHostControllerUSBTransport, 16);
    OSMetaClassDeclareReservedUnused(IntelGen1BluetoothHostControllerUSBTransport, 17);
    OSMetaClassDeclareReservedUnused(IntelGen1BluetoothHostControllerUSBTransport, 18);
    OSMetaClassDeclareReservedUnused(IntelGen1BluetoothHostControllerUSBTransport, 19);
    OSMetaClassDeclareReservedUnused(IntelGen1BluetoothHostControllerUSBTransport, 20);
    OSMetaClassDeclareReservedUnused(IntelGen1BluetoothHostControllerUSBTransport, 21);
    OSMetaClassDeclareReservedUnused(IntelGen1BluetoothHostControllerUSBTransport, 22);
    OSMetaClassDeclareReservedUnused(IntelGen1BluetoothHostControllerUSBTransport, 23);

protected:
    BluetoothHCIEventPacketHeader * mRequiredEvent;
    UInt8 * mRequiredEventParams;
    BluetoothHCICommandOpCode mCurrentCommandOpCode;
    bool mReceivedEventValid;
    bool mPatching;
    bool mIsDefaultFirmware;
};

#endif

