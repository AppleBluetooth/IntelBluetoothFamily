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

#ifndef IntelGen2BluetoothHostController_h
#define IntelGen2BluetoothHostController_h

#include "IntelBluetoothHostController.h"

class IntelGen2BluetoothHostController : public IntelBluetoothHostController
{
    OSDeclareDefaultStructors(IntelGen2BluetoothHostController)
    
public:
    virtual bool start(IOService * provider) APPLE_KEXT_OVERRIDE;
    
    virtual IOReturn GetFirmwareNameWL(void * version, BluetoothIntelBootParams * params, const char * suffix, char * fwName) APPLE_KEXT_OVERRIDE;
    virtual IOReturn GetFirmwareWL(void * version, BluetoothIntelBootParams * params, const char * suffix, OSData ** fwData) APPLE_KEXT_OVERRIDE;
    virtual IOReturn DownloadFirmwareWL(BluetoothHCIRequestID inID, void * version, BluetoothIntelBootParams * params, UInt32 * bootAddress) APPLE_KEXT_OVERRIDE;
    
    OSMetaClassDeclareReservedUnused(IntelGen2BluetoothHostController, 0);
    OSMetaClassDeclareReservedUnused(IntelGen2BluetoothHostController, 1);
    OSMetaClassDeclareReservedUnused(IntelGen2BluetoothHostController, 2);
    OSMetaClassDeclareReservedUnused(IntelGen2BluetoothHostController, 3);
    OSMetaClassDeclareReservedUnused(IntelGen2BluetoothHostController, 4);
    OSMetaClassDeclareReservedUnused(IntelGen2BluetoothHostController, 5);
    OSMetaClassDeclareReservedUnused(IntelGen2BluetoothHostController, 6);
    OSMetaClassDeclareReservedUnused(IntelGen2BluetoothHostController, 7);
    OSMetaClassDeclareReservedUnused(IntelGen2BluetoothHostController, 8);
    OSMetaClassDeclareReservedUnused(IntelGen2BluetoothHostController, 9);
    OSMetaClassDeclareReservedUnused(IntelGen2BluetoothHostController, 10);
    OSMetaClassDeclareReservedUnused(IntelGen2BluetoothHostController, 11);
    OSMetaClassDeclareReservedUnused(IntelGen2BluetoothHostController, 12);
    OSMetaClassDeclareReservedUnused(IntelGen2BluetoothHostController, 13);
    OSMetaClassDeclareReservedUnused(IntelGen2BluetoothHostController, 14);
    OSMetaClassDeclareReservedUnused(IntelGen2BluetoothHostController, 15);
    OSMetaClassDeclareReservedUnused(IntelGen2BluetoothHostController, 16);
    OSMetaClassDeclareReservedUnused(IntelGen2BluetoothHostController, 17);
    OSMetaClassDeclareReservedUnused(IntelGen2BluetoothHostController, 18);
    OSMetaClassDeclareReservedUnused(IntelGen2BluetoothHostController, 19);
    OSMetaClassDeclareReservedUnused(IntelGen2BluetoothHostController, 20);
    OSMetaClassDeclareReservedUnused(IntelGen2BluetoothHostController, 21);
    OSMetaClassDeclareReservedUnused(IntelGen2BluetoothHostController, 22);
    OSMetaClassDeclareReservedUnused(IntelGen2BluetoothHostController, 23);
};

#endif
