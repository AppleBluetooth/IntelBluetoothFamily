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

#ifndef IntelBluetoothHostController_h
#define IntelBluetoothHostController_h

#include <IOKit/bluetooth/IOBluetoothHCIController.h>
#include <IOKit/bluetooth/IOBluetoothHCIRequest.h>
#include <IOKit/IOLib.h>
#include "IntelBluetoothHostControllerTypes.h"

class IntelBluetoothHostControllerUSBTransport;

IOBLUETOOTH_EXPORT IOReturn ParseIntelVendorSpecificCommand(UInt16 ocf, UInt8 * inData, UInt32 inDataSize, UInt8 * outData, UInt32 * outDataSize, UInt8 * outStatus);

class IntelBluetoothHostController : public IOBluetoothHostController
{
    OSDeclareDefaultStructors(IntelBluetoothHostController)
    
    friend class IntelBluetoothHostControllerUSBTransport;
    friend class IntelGen1BluetoothHostControllerUSBTransport;
    friend class IntelGen2BluetoothHostControllerUSBTransport;
    friend class IntelGen3BluetoothHostControllerUSBTransport;
    
public:
    /*! @function init
     *   @abstract Initializes data structures (e.g. mExpansionData) and member variables.
     *   @discussion This function overrides the init() specific to IOBluetoothHostController instead of the generic one of IOService.
     *   @param family The IOBluetoothHCIController instance to initialize with.
     *   @param transport The abstract transport instance to initialize with.
     *   @result <code>true</code> if the object is successfully initialized.
     */

    virtual bool init(IOBluetoothHCIController * family, IOBluetoothHostControllerTransport * transport) APPLE_KEXT_OVERRIDE;

    /*! @function free
     *   @abstract Frees data allocated in init.
     */

    virtual void free() APPLE_KEXT_OVERRIDE;

    virtual bool start(IOService * provider) APPLE_KEXT_OVERRIDE;

    virtual bool InitializeController() APPLE_KEXT_OVERRIDE;
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_10_14
    virtual IOReturn SetupController(bool *) APPLE_KEXT_OVERRIDE;
#else
    virtual IOReturn SetupController() APPLE_KEXT_OVERRIDE;
#endif
    virtual IOReturn SetupGen1Controller();
    virtual IOReturn SetupGen2Controller();
    virtual IOReturn SetupGen3Controller();
    virtual bool     InitializeHostControllerVariables(bool setup) APPLE_KEXT_OVERRIDE;

    virtual IOReturn SendHCIRequestFormatted(BluetoothHCIRequestID inID, BluetoothHCICommandOpCode inOpCode, IOByteCount outResultsSize, void * outResultsPtr, const char * inFormat, ...) APPLE_KEXT_OVERRIDE;

    virtual IOReturn SetTransportRadioPowerState(UInt8 inState) APPLE_KEXT_OVERRIDE;
    virtual IOReturn GetTransportRadioPowerState(UInt8 * outState) APPLE_KEXT_OVERRIDE;
    virtual IOReturn CallPowerRadio(bool) APPLE_KEXT_OVERRIDE;

    virtual void SetMicrosoftExtensionOpCode(UInt8 hardwareVariant); // implement in 1.0.1
    virtual IOReturn ResetToBootloader(bool retry);
    virtual IOReturn WriteDeviceAddress(BluetoothHCIRequestID inID, BluetoothDeviceAddress * inAddress) APPLE_KEXT_OVERRIDE;
    virtual IOReturn CheckDeviceAddress();
    virtual IOReturn CallBluetoothHCIIntelReadVersionInfo(UInt8 param);
    virtual IOReturn PrintVersionInfo(BluetoothIntelVersionInfo * version);
    virtual IOReturn PrintVersionInfo(BluetoothIntelVersionInfoTLV * version);
    virtual IOReturn ConfigureOffload(); // implement in 1.0.1
    virtual IOReturn SetQualityReport(bool enable);
    virtual IOReturn SetDebugFeatures(const BluetoothIntelDebugFeatures * features);
    virtual IOReturn ResetDebugFeatures(const BluetoothIntelDebugFeatures * features);
    virtual IOReturn CallBluetoothHCIIntelSetEventMask(bool debug);
    virtual IOReturn CallBluetoothHCIIntelSetDiagnosticMode(bool enable);

    virtual IOReturn WaitForFirmwareDownload(UInt32 callTime, UInt32 deadline);
    virtual IOReturn WaitForDeviceBoot(UInt32 callTime, UInt32 deadline);
    virtual IOReturn BootDevice(UInt32 bootAddress);

    virtual void ProcessEventDataWL(UInt8 * inDataPtr, UInt32 inDataSize, UInt32 sequenceNumber) APPLE_KEXT_OVERRIDE;
    virtual bool GetCompleteCodeForCommand(BluetoothHCICommandOpCode inOpCode, BluetoothHCIEventCode * outEventCode) APPLE_KEXT_OVERRIDE;
    virtual bool SetHCIRequestRequireEvents(BluetoothHCICommandOpCode opCode, IOBluetoothHCIRequest * request) APPLE_KEXT_OVERRIDE;

    virtual IOReturn LoadDDCConfig(OSData * fwData);

    virtual IOReturn BluetoothHCIIntelSecureSend(BluetoothHCIIntelSecureSendFragmentType fragmentType, UInt32 paramSize, const UInt8 * param);
    
    /*! @function BluetoothHCISendIntelReset
     *   @abstract Sends the Intel Reset HCI command.
     *   @discussion This vendor specific HCI command re-enumerates the Bluetooth host controller.
     *   @param resetType  The type of the reset: 0x00 (Soft reset), 0x01 (Hard reset).
     *   @param enablePatch Whether to enable patches in the reset or not.
     *   @param reloadDDC Whether to reload DDC or not.
     *   @param bootOption The boot option: 0x00 (current image), 0x01 (specified boot address).
     *   @param bootAddress The boot address which applies only when bootOptions is 0x01.
     */

    virtual IOReturn BluetoothHCISendIntelReset(BluetoothHCIRequestID inID, UInt8 resetType, bool enablePatch, bool reloadDDC, UInt8 bootOption, UInt32 bootAddress);
    virtual IOReturn BluetoothHCIIntelEnterManufacturerMode(BluetoothHCIRequestID inID);
    virtual IOReturn BluetoothHCIIntelExitManufacturerMode(BluetoothHCIRequestID inID, BluetoothIntelManufacturingExitResetOption resetOption);
    virtual IOReturn BluetoothHCIIntelSetEventMask(BluetoothHCIRequestID inID, bool debug);
    virtual IOReturn BluetoothHCIIntelSetDiagnosticMode(BluetoothHCIRequestID inID, bool enable);
    virtual IOReturn BluetoothHCIIntelReadBootParams(BluetoothHCIRequestID inID, BluetoothIntelBootParams * params);
    virtual IOReturn BluetoothHCIIntelReadVersionInfo(BluetoothHCIRequestID inID, UInt8 param, UInt8 * response);
    virtual IOReturn BluetoothHCIIntelReadDebugFeatures(BluetoothHCIRequestID inID, BluetoothIntelDebugFeatures * features);
    virtual IOReturn BluetoothHCIIntelTurnOffDeviceLED(BluetoothHCIRequestID inID);
    virtual IOReturn BluetoothHCIIntelWriteDDC(BluetoothHCIRequestID inID, UInt8 * data, UInt8 dataSize);
    virtual IOReturn BluetoothHCIIntelReadOffloadUseCases(BluetoothHCIRequestID inID, BluetoothIntelOffloadUseCases * cases);
    virtual IOReturn BluetoothHCIIntelSetLinkStatisticsEventsTracing(BluetoothHCIRequestID inID, UInt8 param);
    virtual IOReturn BluetoothHCIIntelReadExceptionInfo(BluetoothHCIRequestID inID, BluetoothIntelExceptionInfo * info);
    
protected:
    virtual const char * ConvertFirmwareVariantToString(BluetoothHCIIntelFirmwareVariant variant);
    virtual const char * ConvertImageTypeToString(BluetoothHCIIntelImageType imageType);
    virtual IOReturn DownloadFirmwarePayload(OSData * fwData, size_t offset);
    virtual IOReturn SecureSendSFIRSAFirmwareHeader(OSData * fwData);
    virtual IOReturn SecureSendSFIECDSAFirmwareHeader(OSData * fwData);
    virtual bool CheckFirmwareVersion(UInt8 number, UInt8 week, UInt8 year, OSData * fwData, UInt32 * bootAddress);
    
    OSMetaClassDeclareReservedUnused(IntelBluetoothHostController, 0);
    OSMetaClassDeclareReservedUnused(IntelBluetoothHostController, 1);
    OSMetaClassDeclareReservedUnused(IntelBluetoothHostController, 2);
    OSMetaClassDeclareReservedUnused(IntelBluetoothHostController, 3);
    OSMetaClassDeclareReservedUnused(IntelBluetoothHostController, 4);
    OSMetaClassDeclareReservedUnused(IntelBluetoothHostController, 5);
    OSMetaClassDeclareReservedUnused(IntelBluetoothHostController, 6);
    OSMetaClassDeclareReservedUnused(IntelBluetoothHostController, 7);
    OSMetaClassDeclareReservedUnused(IntelBluetoothHostController, 8);
    OSMetaClassDeclareReservedUnused(IntelBluetoothHostController, 9);
    OSMetaClassDeclareReservedUnused(IntelBluetoothHostController, 10);
    OSMetaClassDeclareReservedUnused(IntelBluetoothHostController, 11);
    OSMetaClassDeclareReservedUnused(IntelBluetoothHostController, 12);
    OSMetaClassDeclareReservedUnused(IntelBluetoothHostController, 13);
    OSMetaClassDeclareReservedUnused(IntelBluetoothHostController, 14);
    OSMetaClassDeclareReservedUnused(IntelBluetoothHostController, 15);
    OSMetaClassDeclareReservedUnused(IntelBluetoothHostController, 16);
    OSMetaClassDeclareReservedUnused(IntelBluetoothHostController, 17);
    OSMetaClassDeclareReservedUnused(IntelBluetoothHostController, 18);
    OSMetaClassDeclareReservedUnused(IntelBluetoothHostController, 19);
    OSMetaClassDeclareReservedUnused(IntelBluetoothHostController, 20);
    OSMetaClassDeclareReservedUnused(IntelBluetoothHostController, 21);
    OSMetaClassDeclareReservedUnused(IntelBluetoothHostController, 22);
    OSMetaClassDeclareReservedUnused(IntelBluetoothHostController, 23);

protected:
    UInt8 * mVersionInfo;
    UInt32 mGeneration;
    BluetoothHCICommandOpCode mMicrosoftExtensionOpCode;

    bool mValidLEStates;
    bool mStrictDuplicateFilter;
    bool mSimultaneousDiscovery;
    bool mDiagnosticModeNotPersistent;
    bool mWidebandSpeechSupported;
    bool mInvalidDeviceAddress;
    
    bool mBooting;
    bool mBootloaderMode;
    bool mDownloading;
    bool mFirmwareLoaded;
    bool mFirmwareLoadingFailed;
    bool mQualityReportSet;

    struct ExpansionData
    {
        void * mRefCon;
    };
    ExpansionData * mExpansionData;
};

#endif
