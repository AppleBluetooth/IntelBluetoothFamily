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

    if ( mVendorID == 32903 )
    {
        mControllerVendorType = 8;
        setProperty("ActiveBluetoothControllerVendor", "Intel");
    }
    else
        return false;
        
    IntelBluetoothHostController * controller = OSDynamicCast(IntelBluetoothHostController, mBluetoothController);
    if ( !controller )
        return false;

    /* The some controllers have a bug with the first HCI command sent to it
     * returning number of completed commands as zero. This would stall the
     * command processing in the Bluetooth core.
     *
     * As a workaround, send HCI Reset command first which will reset the
     * number of completed commands and allow normal command processing
     * from now on.
     */

    if ( mProductID == 2012 )
    {
        controller->mBrokenInitialNumberOfCommands = true;
        if ( controller->CallBluetoothHCIReset(false, (char *) __FUNCTION__) )
            return false;
    }

    /* Starting from TyP device, the command parameter and response are
     * changed even though the OCF for HCI_Intel_Read_Version command
     * remains same. The legacy devices can handle even if the
     * command has a parameter and returns a correct version information.
     * So, it uses new format to support both legacy and new format.
     */

    if ( controller->CallBluetoothHCIIntelReadVersionInfo(0xFF) )
        return false;

    /* Apply the common HCI quirks for Intel device */
    controller->mStrictDuplicateFilter = true;
    controller->mSimultaneousDiscovery = true;
    controller->mDiagnosticModeNotPersistent = true;
    
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
    if ( controller->CallBluetoothHCIReset(false, (char *) __FUNCTION__) )
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
    IntelBluetoothHostController * controller = OSDynamicCast(IntelBluetoothHostController, mBluetoothController);
    if ( !controller )
        return kIOReturnError;

    return controller->mCommandGate->runAction(GetFirmwareAction, version, params, (void *) suffix, fwName);
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
    IntelBluetoothHostController * controller = OSDynamicCast(IntelBluetoothHostController, mBluetoothController);
    if ( !controller )
        return kIOReturnError;

    return controller->mCommandGate->runAction(GetFirmwareAction, version, params, (void *) suffix, *fwData);
}

IOReturn IntelBluetoothHostControllerUSBTransport::GetFirmwareAction(OSObject * owner, void * arg0, void * arg1, void * arg2, void * arg3)
{
    IntelBluetoothHostControllerUSBTransport * object = OSDynamicCast(IntelBluetoothHostControllerUSBTransport, owner);
    return object->GetFirmwareWL(arg0, (BluetoothIntelBootParams *) arg1, (const char *) arg2, (OSData **) &arg3);
}

IOReturn IntelBluetoothHostControllerUSBTransport::GetFirmwareWL(void * version, BluetoothIntelBootParams * params, const char * suffix, OSData ** fwData)
{
    return kIOReturnUnsupported;
}

IOReturn IntelBluetoothHostControllerUSBTransport::DownloadFirmware(BluetoothHCIRequestID inID, void * version, BluetoothIntelBootParams * params, UInt32 * bootAddress)
{
    IntelBluetoothHostController * controller = OSDynamicCast(IntelBluetoothHostController, mBluetoothController);
    if ( !controller )
        return kIOReturnError;

    return controller->mCommandGate->runAction(DownloadFirmwareAction, &inID, version, params, bootAddress);
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
