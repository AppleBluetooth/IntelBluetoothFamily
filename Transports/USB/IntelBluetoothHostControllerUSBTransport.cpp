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

#include "IntelBluetoothHostControllerUSBTransport.hpp"

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
    mFirmware->removeFirmwares();
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
    if ( !super::start(provider) )
        return false;

    if ( mVendorID == 0x8087 )
    {
        mControllerVendorType = 8;
        setProperty("ActiveBluetoothControllerVendor", "Intel");
    }
    else
        return false;
    
    mBluetoothUSBHostDevice->retain();
    registerService();
    
    return true;
}

void IntelBluetoothHostControllerUSBTransport::stop(IOService * provider)
{
    BluetoothHCIRequestID id;

    IntelBluetoothHostController * controller = OSDynamicCast(IntelBluetoothHostController, mBluetoothController);
    if ( !controller )
        return;

    /* Send HCI Reset to the controller to stop any BT activity which
     * were triggered. This will help to save power and maintain the
     * sync b/w Host and controller
     */
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_11_0
    if ( controller->CallBluetoothHCIReset(false, (char *) __FUNCTION__) )
#else
    if ( controller->CallBluetoothHCIReset(false) )
#endif
        return;

    /* Some platforms have an issue with BT LED when the interface is
     * down or BT radio is turned off, which takes 5 seconds to BT LED
     * goes off. This command turns off the BT LED immediately.
     */
    if ( controller->mBrokenLED )
    {
        controller->HCIRequestCreate(&id);
        controller->BluetoothHCIIntelTurnOffDeviceLED(id);
        controller->HCIRequestDelete(NULL, id);
    }
    super::stop(provider);
}

IOReturn IntelBluetoothHostControllerUSBTransport::GetFirmwareName(void * version, BluetoothIntelBootParams * params, const char * suffix, char * fwName, IOByteCount size)
{
    return mCommandGate->runAction(GetFirmwareNameAction, version, params, (void *) suffix, fwName);
}

IOReturn IntelBluetoothHostControllerUSBTransport::GetFirmwareNameAction(OSObject * owner, void * arg0, void * arg1, void * arg2, void * arg3)
{
    IntelBluetoothHostControllerUSBTransport * object = OSDynamicCast(IntelBluetoothHostControllerUSBTransport, owner);
    return object->GetFirmwareNameWL(arg0, (BluetoothIntelBootParams *) arg1, (const char *) arg2, (char *) arg3);
}

IOReturn IntelBluetoothHostControllerUSBTransport::GetFirmwareNameWL(void * version, BluetoothIntelBootParams * params, const char * suffix, char * fwName)
{
    return kIOReturnUnsupported;
}

IOReturn IntelBluetoothHostControllerUSBTransport::GetFirmware(void * version, BluetoothIntelBootParams * params, const char * suffix, OSData ** fwData)
{
    char fwName[64];

    if ( GetFirmwareName(version, params, suffix, fwName, sizeof(fwName)) )
    {
        os_log(mInternalOSLogObject, "[IntelBluetoothHostControllerUSBTransport][GetFirmware] Unsupported firmware name!");
        return kIOReturnInvalid;
    }

    setProperty("FirmwareName", fwName);

    mFirmware = OpenFirmwareManager::withName(fwName, mFirmwareCandidates, mNumFirmwares);
    if ( !mFirmware )
    {
        os_log(mInternalOSLogObject, "[IntelBluetoothHostControllerUSBTransport][GetFirmware] Failed to obtain firmware file %s!!!", fwName);
        return GetFirmwareErrorHandler(version, params, suffix, fwData);
    }

    *fwData = mFirmware->getFirmwareUncompressed(fwName);

    os_log(mInternalOSLogObject, "[IntelBluetoothHostControllerUSBTransport][GetFirmware] Found firmware file: %s", fwName);

    return kIOReturnSuccess;
}

IOReturn IntelBluetoothHostControllerUSBTransport::GetFirmwareErrorHandler(void * version, BluetoothIntelBootParams * params, const char * suffix, OSData ** fwData)
{
    return kIOReturnUnsupported;
}

IOReturn IntelBluetoothHostControllerUSBTransport::PatchFirmware(BluetoothHCIRequestID inID, OSData * fwData, UInt8 ** fwPtr, int * disablePatch)
{
    return kIOReturnUnsupported;
}

IOReturn IntelBluetoothHostControllerUSBTransport::DownloadFirmware(BluetoothHCIRequestID inID, void * version, BluetoothIntelBootParams * params, UInt32 * bootAddress)
{
    return mCommandGate->runAction(DownloadFirmwareAction, &inID, version, params, bootAddress);
}

IOReturn IntelBluetoothHostControllerUSBTransport::DownloadFirmwareAction(OSObject * owner, void * arg0, void * arg1, void * arg2, void * arg3)
{
    IntelBluetoothHostControllerUSBTransport * object = OSDynamicCast(IntelBluetoothHostControllerUSBTransport, owner);
    return object->DownloadFirmwareWL(*(BluetoothHCIRequestID *) arg0, arg1, (BluetoothIntelBootParams *) arg2, (UInt32 *) arg3);
}

IOReturn IntelBluetoothHostControllerUSBTransport::DownloadFirmwareWL(BluetoothHCIRequestID inID, void * version, BluetoothIntelBootParams * params, UInt32 * bootAddress)
{
    return kIOReturnUnsupported;
}

IOReturn IntelBluetoothHostControllerUSBTransport::ParseVersionInfoTLV(BluetoothIntelVersionInfoTLV * version, UInt8 * data, IOByteCount dataSize)
{
    return kIOReturnUnsupported;
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
