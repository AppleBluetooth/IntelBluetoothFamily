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

#ifndef IntelBluetoothHostControllerUSBTransport_h
#define IntelBluetoothHostControllerUSBTransport_h

#include <IOKit/bluetooth/transport/IOBluetoothHostControllerUSBTransport.h>
#include <OpenFirmwareManager.h>
#include "IntelFirmwareList.h"
#include "../IntelBluetoothFamily/IntelBluetoothHostController.h"

class IntelBluetoothHostControllerUSBTransport : public IOBluetoothHostControllerUSBTransport
{
    OSDeclareDefaultStructors(IntelBluetoothHostControllerUSBTransport)

public:
    virtual bool init( OSDictionary * dictionary = NULL ) APPLE_KEXT_OVERRIDE;
    virtual void free() APPLE_KEXT_OVERRIDE;
    virtual IOService * probe( IOService * provider, SInt32 * score ) APPLE_KEXT_OVERRIDE;
    virtual bool start( IOService * provider ) APPLE_KEXT_OVERRIDE;
    virtual void stop( IOService * provider ) APPLE_KEXT_OVERRIDE;
    
    virtual IOReturn setFirmware(char * fwName);
    virtual OpenFirmwareManager * getFirmware();
    
    OSMetaClassDeclareReservedUnused(IntelBluetoothHostControllerUSBTransport, 0);
    OSMetaClassDeclareReservedUnused(IntelBluetoothHostControllerUSBTransport, 1);
    OSMetaClassDeclareReservedUnused(IntelBluetoothHostControllerUSBTransport, 2);
    OSMetaClassDeclareReservedUnused(IntelBluetoothHostControllerUSBTransport, 3);
    OSMetaClassDeclareReservedUnused(IntelBluetoothHostControllerUSBTransport, 4);
    OSMetaClassDeclareReservedUnused(IntelBluetoothHostControllerUSBTransport, 5);
    OSMetaClassDeclareReservedUnused(IntelBluetoothHostControllerUSBTransport, 6);
    OSMetaClassDeclareReservedUnused(IntelBluetoothHostControllerUSBTransport, 7);
    OSMetaClassDeclareReservedUnused(IntelBluetoothHostControllerUSBTransport, 8);
    OSMetaClassDeclareReservedUnused(IntelBluetoothHostControllerUSBTransport, 9);
    OSMetaClassDeclareReservedUnused(IntelBluetoothHostControllerUSBTransport, 10);
    OSMetaClassDeclareReservedUnused(IntelBluetoothHostControllerUSBTransport, 11);
    OSMetaClassDeclareReservedUnused(IntelBluetoothHostControllerUSBTransport, 12);
    OSMetaClassDeclareReservedUnused(IntelBluetoothHostControllerUSBTransport, 13);
    OSMetaClassDeclareReservedUnused(IntelBluetoothHostControllerUSBTransport, 14);
    OSMetaClassDeclareReservedUnused(IntelBluetoothHostControllerUSBTransport, 15);
    OSMetaClassDeclareReservedUnused(IntelBluetoothHostControllerUSBTransport, 16);
    OSMetaClassDeclareReservedUnused(IntelBluetoothHostControllerUSBTransport, 17);
    OSMetaClassDeclareReservedUnused(IntelBluetoothHostControllerUSBTransport, 18);
    OSMetaClassDeclareReservedUnused(IntelBluetoothHostControllerUSBTransport, 19);
    OSMetaClassDeclareReservedUnused(IntelBluetoothHostControllerUSBTransport, 20);
    OSMetaClassDeclareReservedUnused(IntelBluetoothHostControllerUSBTransport, 21);
    OSMetaClassDeclareReservedUnused(IntelBluetoothHostControllerUSBTransport, 22);
    OSMetaClassDeclareReservedUnused(IntelBluetoothHostControllerUSBTransport, 23);
    
protected:
    OpenFirmwareManager * mFirmware;
    
    struct ExpansionData
    {
        void * mRefCon;
    };
    ExpansionData * mExpansionData;
};

#endif
