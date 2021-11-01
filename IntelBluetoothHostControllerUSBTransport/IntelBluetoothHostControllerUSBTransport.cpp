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

#include "IntelBluetoothHostControllerUSBTransport.h"

#define super IOBluetoothHostControllerUSBTransport
OSDefineMetaClassAndStructors(IntelBluetoothHostControllerUSBTransport, super)

bool IntelBluetoothHostControllerUSBTransport::init( OSDictionary * dictionary )
{
    CreateOSLogObject();
    if ( !super::init() )
    {
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostControllerUSBTransport][init] -- super::init() failed ****\n");
        return false;
    }
    
    mExpansionData = IONewZero(ExpansionData, 1);
    if ( !mExpansionData )
        return false;
    
    mFirmware = NULL;
    return true;
}

void IntelBluetoothHostControllerUSBTransport::free()
{
    IOSafeDeleteNULL(mExpansionData, ExpansionData, 1);
    mFirmware->removeFirmware();
    OSSafeReleaseNULL(mFirmware);
    super::free();
}

IOService * IntelBluetoothHostControllerUSBTransport::probe( IOService * provider, SInt32 * score )
{
    IOService * result = super::probe(provider, score);
    IOUSBHostDevice * device;
    UInt16 vendorID;
    UInt16 productID;
    
    device = OSDynamicCast(IOUSBHostDevice, provider);
    if ( !device )
        return NULL;
    
    vendorID = device->getDeviceDescriptor()->idVendor;
    productID = device->getDeviceDescriptor()->idProduct;
    os_log(mInternalOSLogObject, "**** [IntelBluetoothHostControllerUSBTransport][probe] -- USB device info: name: %s, vendor ID: 0x%04X, product ID: 0x%04X\n", device->getName(), vendorID, productID);
    
    return result;
}

bool IntelBluetoothHostControllerUSBTransport::start(IOService * provider)
{
    if (!super::start(provider))
        return false;
        
    IntelBluetoothHostController * controller = OSDynamicCast(IntelBluetoothHostController, mBluetoothController);
    if ( !controller )
        return false;
    
    if ( mProductID == 2012 )
        controller->mExpansionData->mBrokenInitialNumberOfCommands = true;;
    
    OSNumber * deviceGeneration = OSDynamicCast(OSNumber, getProperty("deviceGeneration"));
    switch (deviceGeneration->unsigned32BitValue())
    {
        case 1:
            mControllerVendorType = 8;
            setProperty("ActiveBluetoothControllerVendor", "Intel - Legacy ROM");
            break;
        case 2:
            mControllerVendorType = 9;
            setProperty("ActiveBluetoothControllerVendor", "Intel - Legacy Bootloader");
            break;
        case 3:
            mControllerVendorType = 10;
            setProperty("ActiveBluetoothControllerVendor", "Intel - New Bootloader");
            break;
        default:
            return false;
    }
    
    mBluetoothUSBHostDevice->retain();
    registerService();
    
    return true;
}

IOReturn IntelBluetoothHostControllerUSBTransport::setFirmware(char * fwName)
{
    if (mFirmware)
    {
        mFirmware->removeFirmware();
        OSSafeReleaseNULL(mFirmware);
    }
    mFirmware = OpenFirmwareManager::withName(fwName, fwCandidates, fwCount);
    return kIOReturnSuccess;
}

OpenFirmwareManager * IntelBluetoothHostControllerUSBTransport::getFirmware()
{
    if (mFirmware)
        return mFirmware;
    return NULL;
}

void IntelBluetoothHostControllerUSBTransport::stop(IOService * provider)
{
    super::stop(provider);
}

OSMetaClassDefineReservedUnused(IntelBluetoothHostControllerUSBTransport, 0)
OSMetaClassDefineReservedUnused(IntelBluetoothHostControllerUSBTransport, 1)
OSMetaClassDefineReservedUnused(IntelBluetoothHostControllerUSBTransport, 2)
OSMetaClassDefineReservedUnused(IntelBluetoothHostControllerUSBTransport, 3)
OSMetaClassDefineReservedUnused(IntelBluetoothHostControllerUSBTransport, 4)
OSMetaClassDefineReservedUnused(IntelBluetoothHostControllerUSBTransport, 5)
OSMetaClassDefineReservedUnused(IntelBluetoothHostControllerUSBTransport, 6)
OSMetaClassDefineReservedUnused(IntelBluetoothHostControllerUSBTransport, 7)
OSMetaClassDefineReservedUnused(IntelBluetoothHostControllerUSBTransport, 8)
OSMetaClassDefineReservedUnused(IntelBluetoothHostControllerUSBTransport, 9)
OSMetaClassDefineReservedUnused(IntelBluetoothHostControllerUSBTransport, 10)
OSMetaClassDefineReservedUnused(IntelBluetoothHostControllerUSBTransport, 11)
OSMetaClassDefineReservedUnused(IntelBluetoothHostControllerUSBTransport, 12)
OSMetaClassDefineReservedUnused(IntelBluetoothHostControllerUSBTransport, 13)
OSMetaClassDefineReservedUnused(IntelBluetoothHostControllerUSBTransport, 14)
OSMetaClassDefineReservedUnused(IntelBluetoothHostControllerUSBTransport, 15)
OSMetaClassDefineReservedUnused(IntelBluetoothHostControllerUSBTransport, 16)
OSMetaClassDefineReservedUnused(IntelBluetoothHostControllerUSBTransport, 17)
OSMetaClassDefineReservedUnused(IntelBluetoothHostControllerUSBTransport, 18)
OSMetaClassDefineReservedUnused(IntelBluetoothHostControllerUSBTransport, 19)
OSMetaClassDefineReservedUnused(IntelBluetoothHostControllerUSBTransport, 20)
OSMetaClassDefineReservedUnused(IntelBluetoothHostControllerUSBTransport, 21)
OSMetaClassDefineReservedUnused(IntelBluetoothHostControllerUSBTransport, 22)
OSMetaClassDefineReservedUnused(IntelBluetoothHostControllerUSBTransport, 23)
