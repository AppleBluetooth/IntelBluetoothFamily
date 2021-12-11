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

#include <OpenFirmwareManager.h>
#include <IOKit/bluetooth/transport/IOBluetoothHostControllerUSBTransport.h>
#include "../../HostController/IntelBluetoothHostController.h"

class IntelBluetoothHostControllerUSBTransport : public IOBluetoothHostControllerUSBTransport
{
    OSDeclareDefaultStructors(IntelBluetoothHostControllerUSBTransport)

public:
    virtual bool init( OSDictionary * dictionary = NULL ) APPLE_KEXT_OVERRIDE;
    virtual void free() APPLE_KEXT_OVERRIDE;
    virtual IOService * probe( IOService * provider, SInt32 * score ) APPLE_KEXT_OVERRIDE;
    virtual bool start( IOService * provider ) APPLE_KEXT_OVERRIDE;
    virtual void stop( IOService * provider ) APPLE_KEXT_OVERRIDE;

	virtual bool InitializeTransportWL(IOService * provider) APPLE_KEXT_OVERRIDE;
	virtual IOReturn SendHCIRequest(UInt8 * buffer, IOByteCount size) APPLE_KEXT_OVERRIDE;

	virtual void InjectCommandCompleteEvent(BluetoothHCICommandOpCode opCode);

	virtual bool PostSecureSendBulkPipeRead();
	static void SecureSendBulkInReadHandler(void * owner, void * parameter, IOReturn status, uint32_t bytesTransferred);

	virtual IOReturn TransportSecureSendBulkOutWrite(UInt8 * buffer, UInt32 size);
	virtual IOReturn SecureSendBulkOutWrite(IOMemoryDescriptor * memDescriptor);

	virtual bool SupportNewIdlePolicy() APPLE_KEXT_OVERRIDE;
	virtual bool ConfigurePM(IOService * provider) APPLE_KEXT_OVERRIDE;
	virtual IOReturn CallPowerManagerChangePowerStateTo(unsigned long ordinal, char *) APPLE_KEXT_OVERRIDE;
	
	virtual void systemWillShutdownWL(IOOptionBits options, void * parameter) APPLE_KEXT_OVERRIDE;
	virtual bool ControllerSupportWoBT() APPLE_KEXT_OVERRIDE;

	virtual UInt8 GetRadioPowerState() APPLE_KEXT_OVERRIDE;
	virtual void SetRadioPowerState(UInt8 state) APPLE_KEXT_OVERRIDE;

	virtual bool SearchForUSBCompositeDriver(IOUSBHostDevice * device);
	virtual bool CompositeDeviceAppears();

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_10_14
	virtual bool NeedToTurnOnUSBDebug() APPLE_KEXT_OVERRIDE;
#endif

    virtual IOReturn GetFirmwareName(void * version, BluetoothIntelBootParams * params, const char * suffix, char * fwName, IOByteCount size);
    static IOReturn GetFirmwareNameAction(OSObject * owner, void * arg0, void * arg1, void * arg2, void * arg3);
    virtual IOReturn GetFirmwareNameWL(void * version, BluetoothIntelBootParams * params, const char * suffix, char * fwName);
    virtual IOReturn GetFirmware(void * version, BluetoothIntelBootParams * params, const char * suffix, OSData ** fwData);
    virtual IOReturn GetFirmwareErrorHandler(void * version, BluetoothIntelBootParams * params, const char * suffix, OSData ** fwData);
    virtual IOReturn PatchFirmware(OSData * fwData, UInt8 ** fwPtr, int * disablePatch);
    virtual IOReturn DownloadFirmware(void * version, BluetoothIntelBootParams * params, UInt32 * bootAddress);
    static IOReturn DownloadFirmwareAction(OSObject * owner, void * arg0, void * arg1, void * arg2, void * arg3);
    virtual IOReturn DownloadFirmwareWL(void * version, BluetoothIntelBootParams * params, UInt32 * bootAddress);

    virtual IOReturn ParseVersionInfoTLV(BluetoothIntelVersionInfoTLV * version, UInt8 * data, IOByteCount dataSize);

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
    OpenFirmwareManager *      mFirmware;
	FirmwareDescriptor *       mFirmwareCandidates;
	int	                       mNumFirmwares;
	UInt8	   				   mRadioPowerState;

    struct ExpansionData
    {
        void * mRefCon;
    };
    ExpansionData * mExpansionData;
};

#endif
