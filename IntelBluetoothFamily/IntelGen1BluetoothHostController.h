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

#ifndef IntelGen1BluetoothHostController_h
#define IntelGen1BluetoothHostController_h

#include "IntelBluetoothHostController.h"

class IntelGen1BluetoothHostController : public IntelBluetoothHostController
{
    OSDeclareDefaultStructors(IntelGen1BluetoothHostController)
    
public:
    virtual bool start(IOService * provider) APPLE_KEXT_OVERRIDE;
    
    virtual IOReturn GetFirmwareNameWL(void * version, BluetoothIntelBootParams * params, const char * suffix, char * fwName) APPLE_KEXT_OVERRIDE;
    virtual IOReturn GetFirmwareWL(void * version, BluetoothIntelBootParams * params, const char * suffix, OSData ** fwData) APPLE_KEXT_OVERRIDE;
    virtual IOReturn PatchFirmware(BluetoothHCIRequestID inID, OSData * fwData, UInt8 ** fwPtr, int * disablePatch);
    virtual IOReturn LoadDDCConfig(BluetoothHCIRequestID inID, OSData * fwData) APPLE_KEXT_OVERRIDE;
    
    OSMetaClassDeclareReservedUnused(IntelGen1BluetoothHostController, 0);
    OSMetaClassDeclareReservedUnused(IntelGen1BluetoothHostController, 1);
    OSMetaClassDeclareReservedUnused(IntelGen1BluetoothHostController, 2);
    OSMetaClassDeclareReservedUnused(IntelGen1BluetoothHostController, 3);
    OSMetaClassDeclareReservedUnused(IntelGen1BluetoothHostController, 4);
    OSMetaClassDeclareReservedUnused(IntelGen1BluetoothHostController, 5);
    OSMetaClassDeclareReservedUnused(IntelGen1BluetoothHostController, 6);
    OSMetaClassDeclareReservedUnused(IntelGen1BluetoothHostController, 7);
    OSMetaClassDeclareReservedUnused(IntelGen1BluetoothHostController, 8);
    OSMetaClassDeclareReservedUnused(IntelGen1BluetoothHostController, 9);
    OSMetaClassDeclareReservedUnused(IntelGen1BluetoothHostController, 10);
    OSMetaClassDeclareReservedUnused(IntelGen1BluetoothHostController, 11);
    OSMetaClassDeclareReservedUnused(IntelGen1BluetoothHostController, 12);
    OSMetaClassDeclareReservedUnused(IntelGen1BluetoothHostController, 13);
    OSMetaClassDeclareReservedUnused(IntelGen1BluetoothHostController, 14);
    OSMetaClassDeclareReservedUnused(IntelGen1BluetoothHostController, 15);
    OSMetaClassDeclareReservedUnused(IntelGen1BluetoothHostController, 16);
    OSMetaClassDeclareReservedUnused(IntelGen1BluetoothHostController, 17);
    OSMetaClassDeclareReservedUnused(IntelGen1BluetoothHostController, 18);
    OSMetaClassDeclareReservedUnused(IntelGen1BluetoothHostController, 19);
    OSMetaClassDeclareReservedUnused(IntelGen1BluetoothHostController, 20);
    OSMetaClassDeclareReservedUnused(IntelGen1BluetoothHostController, 21);
    OSMetaClassDeclareReservedUnused(IntelGen1BluetoothHostController, 22);
    OSMetaClassDeclareReservedUnused(IntelGen1BluetoothHostController, 23);

protected:
    bool mIsDefaultFirmware;
};

#endif

