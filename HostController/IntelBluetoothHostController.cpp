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

#include <sys/proc.h>
#include "IntelBluetoothHostController.h"
#include "../Transports/Gen1/IntelGen1BluetoothHostControllerUSBTransport.h"
#include "../Transports/Gen2/IntelGen2BluetoothHostControllerUSBTransport.h"
#include "../Transports/Gen3/IntelGen3BluetoothHostControllerUSBTransport.h"

#define super IOBluetoothHostController
OSDefineMetaClassAndStructors(IntelBluetoothHostController, super)

bool IntelBluetoothHostController::init(IOBluetoothHCIController * family, IOBluetoothHostControllerTransport * transport)
{
    CreateOSLogObject();
    if ( !super::init(family, transport) )
        return false;
    mExpansionData = IONewZero(ExpansionData, 1);
    if ( !mExpansionData )
        return false;
    mVersionInfo = IONewZero(UInt8, kMaxHCIBufferLength * 4);

    mValidLEStates = false;
    mStrictDuplicateFilter = false;
    mSimultaneousDiscovery = false;
    mDiagnosticModeNotPersistent = false;
    mWidebandSpeechSupported = false;
    mInvalidDeviceAddress = false;
    mIsLegacyROMDevice = false;
    mBootloaderMode = false;
    mBooting = false;
    mDownloading = false;
    mFirmwareLoaded = false;
    mFirmwareLoadingFailed = false;
    mBrokenLED = false;
    mBrokenInitialNumberOfCommands = false;
    mQualityReportSet = true;
    return true;
}

void IntelBluetoothHostController::free()
{
    IOSafeDeleteNULL(mVersionInfo, UInt8, kMaxHCIBufferLength * 4);
    IOSafeDeleteNULL(mExpansionData, ExpansionData, 1);
    super::free();
}

bool IntelBluetoothHostController::start(IOService * provider)
{
    if ( !super::start(provider) )
        return false;
    mSupportConcurrentCreateConnection = false;
    return true;
}

bool IntelBluetoothHostController::InitializeController()
{
    mBluetoothTransport->SetRemoteWakeUp(true);
    mControllerPowerOptions |= 0x0F;
    SetControllerPowerOptions(mControllerPowerOptions);
    SetControllerFeatureFlags(GetControllerFeatureFlags() | 0x06);
    return true;
}

IOReturn IntelBluetoothHostController::SendHCIRequestFormatted(BluetoothHCIRequestID inID, BluetoothHCICommandOpCode inOpCode, IOByteCount outResultsSize, void * outResultsPtr, const char * inFormat, ...)
{
    va_list va;
    IOReturn err = kIOReturnSuccess;
    BluetoothHCIRequestID id;
    IOBluetoothHCIRequest * request;
    IOBluetoothHCIControllerInternalPowerState state;
    int PID = 0xFF;
    char processName[0x100];
    char opStr[100];
    char errStrLong[100];
    char errStrShort[50];

    va_start(va, inFormat);
    snprintf(processName, 0x100, "Unknown");
    mBluetoothFamily->ConvertOpCodeToString(inOpCode, opStr);

    if ( mSupportNewIdlePolicy )
        ChangeIdleTimerTime((char *) __FUNCTION__, mIdleTimerTime);

    err = LookupRequest(inID, &request);
    if ( err || !request )
    {
        os_log(OS_LOG_DEFAULT, "**** [IntelBluetoothHostController][SendHCIRequestFormatted] ### ERROR: request could not be found! ****\n");
        goto OVER;
    }

    request->RetainRequest((char *) "IntelBluetoothHostController::SendHCIRequestFormatted -- at the beginning");
    request->InitializeRequest();
    PID = request->mPID;
    proc_name(PID, processName, 0x100);

    if ( inOpCode == BluetoothHCIMakeCommandOpCode(kBluetoothHCICommandGroupLowEnergy, kBluetoothHCICommandLESetAdvertisingData) )
        UpdateLESetAdvertisingDataReporter(request);

    if ( !mBluetoothTransport || mBluetoothTransport->isInactive() )
    {
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][SendHCIRequestFormatted] -- Transport is inactive -- inOpCode = 0x%04X (%s), From: %s (%d), mNumberOfCommandsAllowedByHardware is %d, requestPtr = 0x%04x -- this = 0x%04x ****\n", inOpCode, opStr, processName, PID, mNumberOfCommandsAllowedByHardware, ConvertAddressToUInt32(request), ConvertAddressToUInt32(this));
        goto OVER_RELEASE;
    }

    if ( TransportRadioPowerOff(inOpCode, processName, PID, request) )
        goto OVER_RELEASE;

    if ( !request->mAsyncNotify )
        request->SetResultsBufferPtrAndSize((UInt8 *) outResultsPtr, outResultsSize);

    request->mOpCode = inOpCode;
    request->mCommandBufferSize = PackDataList(request->mCommandBuffer, sizeof(request->mCommandBuffer), inFormat, va);

    err = kIOReturnError;
    if ( request->mCommandBufferSize > kMaxHCIBufferLength )
        goto OVER_RELEASE;

    SetHCIRequestRequireEvents(inOpCode, request);

    if ( GetTransportCurrentPowerState(&state) )
    {
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][SendHCIRequestFormatted] -- Returned Error -- GetTransportCurrentPowerState() returned error -- mNumberOfCommandsAllowedByHardware is %d, inID = %d, opCode = 0x%04x, requestPtr = 0x%04x ****\n", mNumberOfCommandsAllowedByHardware, inID, inOpCode, ConvertAddressToUInt32(request));
        goto OVER_RELEASE;
    }

    if ( state != kIOBluetoothHCIControllerInternalPowerStateSleep || request->mAsyncNotify )
    {
        if ( inOpCode == BluetoothHCIMakeCommandOpCode(kBluetoothHCICommandGroupLowEnergy, kBluetoothHCICommandLEStartEncryption) )
            request->mConnectionHandle = *(BluetoothConnectionHandle *) (request->mCommandBuffer + 3);

        err = EnqueueRequest(request);
        if ( err != 99 )
        {
            if ( err )
            {
              mBluetoothFamily->ConvertErrorCodeToString(err, errStrLong, errStrShort);
              os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][SendHCIRequestFormatted] ### ERROR: EnqueueRequest failed (err=0x%x (%s)) for opCode 0x%04x (%s) ****\n", err, errStrLong, inOpCode, opStr);
            }

            err = EnqueueRequestForController(request);
            if ( err )
            {
                AbortRequestAndSetTime(request);
                mBluetoothFamily->ConvertErrorCodeToString(err, errStrLong, errStrShort);
                os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][SendHCIRequestFormatted] ### ERROR: EnqueueRequestForController failed (err=0x%x (%s)) for opCode 0x%04x (%s) ****\n", err, errStrLong, inOpCode, opStr);
                goto OVER_RELEASE;
            }
        }

        if ( (request->mControlFlags & 4) && !HCIRequestCreate(&id, true) )
        {
            BluetoothHCIWriteAuthenticationEnable(id, 0);
            HCIRequestDelete(NULL, id);
        }

        if ( (request->mControlFlags & 8) && !HCIRequestCreate(&id, true) )
        {
            BluetoothHCIWritePageTimeout(id, 10000);
            HCIRequestDelete(NULL, id);
        }

        request->Start();
        err = request->mStatus;
        if ( err <= kBluetoothSyncHCIRequestTimedOutWaitingToBeSent )
        {
            if ( err == kBluetoothSyncHCIRequestTimedOutWaitingToBeSent && !mBusyQueueHead )
            {
                BluetoothFamilyLogPacket(mBluetoothFamily, 249, "**** [IntelBluetoothHostController][SendHCIRequestFormatted] -- requestPtr->Start() returned kBluetoothSyncHCIRequestTimedOutWaitingToBeSent but mBusyQueueHead is NULL -- inID = %d, opCode = 0x%04x (%s), mNumberOfCommandsAllowedByHardware is %d ****\n", inID, inOpCode, opStr, mNumberOfCommandsAllowedByHardware);
            }
            if ( request->mState == kHCIRequestStateWaiting )
            {
                AbortRequestAndSetTime(request);
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_11_0
                IncrementHCICommandTimeOutCounter(request->GetCommandOpCode());
#else
                IncrementHCICommandTimeOutCounter();
#endif
            }
        }
        goto OVER_RELEASE;
    }

    if ( state == kIOBluetoothHCIControllerInternalPowerStateSleep )
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][SendHCIRequestFormatted] -- Returned Error -- Current power state is SLEEP -- cannot send out the HCI command -- mNumberOfCommandsAllowedByHardware is %d, inID = %d, opCode = 0x%04x, requestPtr = 0x%04x ****\n", mNumberOfCommandsAllowedByHardware, inID, inOpCode, ConvertAddressToUInt32(request));

OVER_RELEASE:
    request->ReleaseRequest((char *) "IntelBluetoothHostController::SendHCIRequestFormatted -- before exiting");
OVER:
    if ( mSupportNewIdlePolicy )
        ChangeIdleTimerTime((char *) __FUNCTION__, mIdleTimerTime);

    va_end(va);
    return err;
}

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_10_14
IOReturn IntelBluetoothHostController::SetupController(bool * hardReset)
#else
IOReturn IntelBluetoothHostController::SetupController()
#endif
{
    IOReturn err;
    setConfigState(kIOBluetoothHCIControllerConfigStateKernelSetupPending);

    /* Some controllers have a bug with the first HCI command sent to it
     * returning number of completed commands as zero. This would stall the
     * command processing in the Bluetooth core.
     *
     * As a workaround, send HCI Reset command first which will reset the
     * number of completed commands and allow normal command processing.
     */

    if ( mProductID == 2012 )
    {
        mBrokenInitialNumberOfCommands = true;
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_11_0
        err = CallBluetoothHCIReset(false, (char *) __FUNCTION__);
#else
        err = CallBluetoothHCIReset(false);
#endif
        if ( err )
        {
            REQUIRE_NO_ERR(err);
            return err;
        }
    }

    setConfigState(kIOBluetoothHCIControllerConfigStateKernelPostResetSetupPending);

    /* Starting from TyP devices, the command parameter and response are
     * changed even though the OCF for HCI_Intel_Read_Version command
     * remains same. The legacy devices can handle even if the command
     * has a parameter and returns a correct version information. So,
     * the new format is used to support both legacy and new devices.
     */
    err = CallBluetoothHCIIntelReadVersionInfo(0xFF);
    if ( err )
        return err;

    mStrictDuplicateFilter = true;
    mSimultaneousDiscovery = true;
    mDiagnosticModeNotPersistent = true;

    BluetoothIntelVersionInfo * version = (BluetoothIntelVersionInfo *) mVersionInfo;

    if ( version->hardwarePlatform == 0x37 )
    {
        PrintVersionInfo(version);

        switch ( version->hardwareVariant )
        {
            case kBluetoothIntelHardwareVariantWP:
            case kBluetoothIntelHardwareVariantStP:
                err = SetupGen1Controller();
                break;

            case kBluetoothIntelHardwareVariantSfP:
            case kBluetoothIntelHardwareVariantWsP:
            case kBluetoothIntelHardwareVariantJfP:
            case kBluetoothIntelHardwareVariantThP:
            case kBluetoothIntelHardwareVariantHrP:
            case kBluetoothIntelHardwareVariantCcP:
SETUP_GEN2:
                err = SetupGen2Controller();
                break;

            default:
                os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][SetupController] -- Unsupported hardware variant: %u ****\n", version->hardwareVariant);
                err = kIOReturnInvalid;
        }

        if ( err )
        {
            REQUIRE_NO_ERR(err);
            return err;
        }
        goto COMPLETE;
    }

    err = SetupGen3Controller();
    if ( err == kIOReturnUnsupported && mGeneration == 2 )
    {
        /* Some legacy bootloader devices from JfP supports both old
         * and TLV based HCI_Intel_Read_Version command. But we don't
         * want to use the TLV based setup routines for those legacy
         * bootloader devices.
         *
         * Also, it is not easy to convert TLV based version from the
         * legacy version format.
         *
         * So, as a workaround for those devices, use the legacy
         * HCI_Intel_Read_Version to get the version information and
         * run the legacy bootloader setup.
         */
        err = CallBluetoothHCIIntelReadVersionInfo(0x00);
        if ( err )
            return err;
        goto SETUP_GEN2;
    }
    else if ( err )
    {
        REQUIRE_NO_ERR(err);
        return err;
    }

COMPLETE:
    /* Set the event mask for Intel specific vendor events. This enables
     * a few extra events that are useful during general operation. It
     * does not enable any debugging related events.
     *
     * The device will function correctly without these events enabled
     * and thus no need to fail the setup.
     */
    CallBluetoothHCIIntelSetEventMask(false);

    err = SetupGeneralController();
    if ( err && mBluetoothTransport )
    {
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_10_14
        if ( hardReset )
            *hardReset = true;
#endif
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_10_15
        err = HardResetController(1);
#else
        err = BluetoothResetDevice(1);
#endif
    }

    return err;
}

IOReturn IntelBluetoothHostController::SetupGen1Controller()
{
    IOReturn err;
    BluetoothIntelVersionInfo * version = (BluetoothIntelVersionInfo *) mVersionInfo;
    OSData * fwData;
    UInt8 * fwPtr;
    BluetoothHCIRequestID id;
    int disablePatch;
    IntelGen1BluetoothHostControllerUSBTransport * transport = (IntelGen1BluetoothHostControllerUSBTransport *) mBluetoothTransport;
    if ( !transport )
    {
        REQUIRE("( transport != NULL )");
        return kIOReturnError;
    }

    mGeneration = 1;
    mIsLegacyROMDevice = true;

    /* Apply the device specific HCI quirks
     *
     * WBS for SdP - SdP and Stp have a same hw_varaint but
     * different fw_variant
     */
    if ( version->hardwareVariant == kBluetoothIntelHardwareVariantStP && version->firmwareVariant == kBluetoothHCIIntelFirmwareVariantLegacyROM2_X )
        mWidebandSpeechSupported = true;

    /* These devices have an issue with LED which doesn't
     * go off immediately during shutdown. Set the flag
     * here to send the LED OFF command during shutdown.
     */
    mBrokenLED = true;

    /* fw_patch_num indicates the version of patch the device currently
     * have. If there is no patch data in the device, it is always 0x00.
     * So, if it is other than 0x00, no need to patch the device again.
     */
    if ( version->firmwarePatchVersion )
    {
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][SetupGen1Controller] -- Device is already patched -- patch number: %02x ****\n", version->firmwarePatchVersion);
        goto complete;
    }

    /* Opens the firmware patch file based on the firmware version read
     * from the controller. If it fails to open the matching firmware
     * patch file, it tries to open the default firmware patch file.
     * If no patch file is found, allow the device to operate without
     * a patch.
     */
    transport->GetFirmware(version, NULL, "bseq", &fwData);
    if ( !fwData )
        goto complete;
    fwPtr = (UInt8 *) fwData->getBytesNoCopy();

    /* Enable the manufacturer mode of the controller.
     * Only while this mode is enabled, the driver can download the
     * firmware patch data and configuration parameters.
     */

    err = HCIRequestCreate(&id);
    if ( err )
    {
        REQUIRE_NO_ERR(err);
        return err;
    }
    err = BluetoothHCIIntelEnterManufacturerMode(id);
    HCIRequestDelete(NULL, id);
    if ( err )
        return err;

    disablePatch = 1;

    /* The firmware data file consists of list of Intel specific HCI
     * commands and its expected events. The first byte indicates the
     * type of the message, either HCI command or HCI event.
     *
     * It reads the command and its expected event from the firmware file,
     * and send to the controller. Once __hci_cmd_sync_ev() returns,
     * the returned event is compared with the event read from the firmware
     * file and it will continue until all the messages are downloaded to
     * the controller.
     *
     * Once the firmware patching is completed successfully,
     * the manufacturer mode is disabled with reset and activating the
     * downloaded patch.
     *
     * If the firmware patching fails, the manufacturer mode is
     * disabled with reset and deactivating the patch.
     *
     * If the default patch file is used, no reset is done when disabling
     * the manufacturer.
     */
    while ( fwData->getLength() > fwPtr - (UInt8 *) fwData->getBytesNoCopy() )
    {
        err = transport->PatchFirmware(fwData, &fwPtr, &disablePatch);

        if ( err )
        {
            /* Patching failed. Disable the manufacturer mode with reset and
             * deactivate the downloaded firmware patches.
             */
            err = HCIRequestCreate(&id);
            if ( err )
            {
                REQUIRE_NO_ERR(err);
                return err;
            }
            err = BluetoothHCIIntelExitManufacturerMode(id, kBluetoothIntelManufacturingExitResetOptionResetDeactivatePatches);
            HCIRequestDelete(NULL, id);
            if ( err )
                return err;

            os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][SetupGen1Controller] -- Firmware patch completed and deactivated. ****\n");
            goto complete;
        }
    }

    if ( disablePatch )
    {
        /* Disable the manufacturer mode without reset */
        err = HCIRequestCreate(&id);
        if ( err )
        {
            REQUIRE_NO_ERR(err);
            return err;
        }
        err = BluetoothHCIIntelExitManufacturerMode(id, kBluetoothIntelManufacturingExitResetOptionsNoReset);
        HCIRequestDelete(NULL, id);
        if ( err )
            return err;

        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][SetupGen1Controller] -- Firmware patch completed. ****\n");
        goto complete;
    }

    /* Patching completed successfully and disable the manufacturer mode
     * with reset and activate the downloaded firmware patches.
     */
    err = HCIRequestCreate(&id);
    if ( err )
    {
        REQUIRE_NO_ERR(err);
        return err;
    }
    err = BluetoothHCIIntelExitManufacturerMode(id, kBluetoothIntelManufacturingExitResetOptionResetActivatePatches);
    HCIRequestDelete(NULL, id);
    if ( err )
        return err;

    /* Need build number for downloaded fw patches in
     * every power-on boot
     */
    err = CallBluetoothHCIIntelReadVersionInfo(0x00);
    if ( err )
        return err;

    os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][SetupGen1Controller] -- Firmware patch (0x%02x) completed and activated. ****\n", ((BluetoothIntelVersionInfo *) mVersionInfo)->firmwarePatchVersion);

complete:
    CheckDeviceAddress();

    return kIOReturnSuccess;
}

IOReturn IntelBluetoothHostController::SetupGen2Controller()
{
    IOReturn err;
    BluetoothIntelVersionInfo * version = (BluetoothIntelVersionInfo *) mVersionInfo;
    BluetoothIntelBootParams params;
    UInt32 bootAddress;
    OSData * fwData;
    IntelGen2BluetoothHostControllerUSBTransport * transport = (IntelGen2BluetoothHostControllerUSBTransport *) mBluetoothTransport;
    if ( !transport )
    {
        REQUIRE("( transport != NULL )");
        return kIOReturnError;
    }

    mGeneration = 2;

    if ( version->hardwareVariant == kBluetoothIntelHardwareVariantJfP || version->hardwareVariant == kBluetoothIntelHardwareVariantThP )
        mValidLEStates = true;

    mWidebandSpeechSupported = true;

    /* Setup MSFT Extension support */
    SetMicrosoftExtensionOpCode(version->hardwareVariant);

    /* Set the default boot parameter to 0x0 and it is updated to
     * SKU specific boot parameter after reading Intel_Write_Boot_Params
     * command while downloading the firmware.
     */
    bootAddress = 0x00000000;

    mBootloaderMode = true;

    err = transport->DownloadFirmware(version, &params, &bootAddress);
    if ( err )
        return err;

    /* controller is already having an operational firmware */
    if ( version->firmwareVariant == kBluetoothHCIIntelFirmwareVariantFirmware )
        return kIOReturnSuccess;

    err = BootDevice(bootAddress);
    if ( err )
        return err;

    mBootloaderMode = false;

    /* Once the device is running in operational mode, it needs to
     * apply the device configuration (DDC) parameters.
     *
     * The device can work without DDC parameters, so even if it
     * fails to load the file, no need to fail the setup.
     */
    err = transport->GetFirmware(version, &params, "ddc", &fwData);
    if ( !err )
        LoadDDCConfig(fwData);

    SetQualityReport(mQualityReportSet);

    /* Read the Intel version information after loading the FW */
    err = CallBluetoothHCIIntelReadVersionInfo(0x00);
    if ( err )
        return err;

    version = (BluetoothIntelVersionInfo *) mVersionInfo;
    PrintVersionInfo(version);

    return kIOReturnSuccess;
}

IOReturn IntelBluetoothHostController::SetupGen3Controller()
{
    IOReturn err;
    BluetoothIntelVersionInfoTLV version;
    OSData * fwData;
    UInt32 bootAddress;

    if ( !mBluetoothTransport )
        return kIOReturnError;

    IntelGen3BluetoothHostControllerUSBTransport * transport = (IntelGen3BluetoothHostControllerUSBTransport *) mBluetoothTransport;
    if ( !transport )
    {
        REQUIRE("( transport != NULL )");
        return kIOReturnError;
    }

    err = transport->ParseVersionInfoTLV(&version, mVersionInfo, kMaxHCIBufferLength * 4);
    if ( err )
    {
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][SetupGen3Controller] -- Failed to parse TLV version information! ****\n");
        return err;
    }

    if ( IntelCNVXExtractHardwarePlatform(version.cnviBT) != 0x37 )
    {
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][SetupGen3Controller] -- Unsupported hardware platform: 0x%02x ****\n", IntelCNVXExtractHardwarePlatform(version.cnviBT));
        return kIOReturnInvalid;
    }

    /* Check for supported iBT hardware variants of this firmware
     * loading method.
     *
     * This check has been put in place to ensure correct forward
     * compatibility options when newer hardware variants come
     * along.
     */
    switch ( IntelCNVXExtractHardwareVariant(version.cnviBT) )
    {
        case kBluetoothIntelHardwareVariantJfP:
        case kBluetoothIntelHardwareVariantThP:
        case kBluetoothIntelHardwareVariantHrP:
        case kBluetoothIntelHardwareVariantCcP:
        {
            os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][SetupGen3Controller] -- This controller is not an Intel new bootloader device!!! ****\n");
            mGeneration = 2;
            return kIOReturnUnsupported;
        }

        case kBluetoothIntelHardwareVariantSlr:
            /* Valid LE States quirk for GfP */
            mValidLEStates = true;
        case kBluetoothIntelHardwareVariantTyP:
        case kBluetoothIntelHardwareVariantSlrF:
        {
            mGeneration = 3;

            /* Display version information of TLV type */
            PrintVersionInfo(&version);

            /* Apply the device specific HCI quirks for TLV based devices
             *
             * All TLV based devices support WBS
             */
            mWidebandSpeechSupported = true;

            /* Setup MSFT Extension support */
            SetMicrosoftExtensionOpCode(IntelCNVXExtractHardwareVariant(version.cnviBT));

            /* Set the default boot parameter to 0x0 and it is updated to
             * SKU specific boot parameter after reading Intel_Write_Boot_Params
             * command while downloading the firmware.
             */
            bootAddress = 0x00000000;

            mBootloaderMode = true;

            err = transport->DownloadFirmware(&version, NULL, &bootAddress);
            if ( err )
                return err;

            /* check if controller is already having an operational firmware */
            if ( version.imageType == kBluetoothHCIIntelImageTypeFirmware )
                return kIOReturnSuccess;

            err = BootDevice(bootAddress);
            if ( err )
                return err;

            mBootloaderMode = false;

            /* Once the device is running in operational mode, it needs to
             * apply the device configuration (DDC) parameters.
             *
             * The device can work without DDC parameters, so even if it
             * fails to load the file, no need to fail the setup.
             */
            err = transport->GetFirmware(&version, NULL, "ddc", &fwData);
            if ( !err )
                LoadDDCConfig(fwData);

            /* Read supported use cases and set callbacks to fetch datapath id */
            ConfigureOffload();

            SetQualityReport(mQualityReportSet);

            /* Read the Intel version information after loading the FW  */
            err = CallBluetoothHCIIntelReadVersionInfo(0xFF);
            if ( err )
                return err;

            err = transport->ParseVersionInfoTLV(&version, mVersionInfo, kMaxHCIBufferLength * 4);
            if ( err )
            {
                os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][SetupGen3Controller] -- Failed to parse TLV version information! ****\n");
                return err;
            }

            PrintVersionInfo(&version);

            return kIOReturnSuccess;
        }

        default:
        {
            os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][SetupGen3Controller] -- Unsupported hardware variant: %u ****\n", IntelCNVXExtractHardwareVariant(version.cnviBT));
            return kIOReturnInvalid;
        }
    }
}

bool IntelBluetoothHostController::InitializeHostControllerVariables(bool setup)
{
    if ( !super::InitializeHostControllerVariables(setup) )
        return false;

    if ( mBluetoothTransport )
        mSupportWoBT = mBluetoothTransport->ControllerSupportWoBT();

    setProperty("ActiveBluetoothControllerVendor", "Intel");
    setProperty("BluetoothVendor", "Intel");
    return true;
}

IOReturn IntelBluetoothHostController::SetTransportRadioPowerState(UInt8 inState)
{
    if ( !mBluetoothTransport )
        return kIOReturnInvalid;
    mBluetoothTransport->SetRadioPowerState(inState);
    return kIOReturnSuccess;
}

IOReturn IntelBluetoothHostController::GetTransportRadioPowerState(UInt8 * outState)
{
    if ( !mBluetoothTransport )
        return kIOReturnInvalid;
    *outState = mBluetoothTransport->GetRadioPowerState();
    return kIOReturnSuccess;
}

IOReturn IntelBluetoothHostController::CallPowerRadio(bool)
{
    return kIOReturnSuccess;
}

void IntelBluetoothHostController::SetMicrosoftExtensionOpCode(UInt8 hardwareVariant)
{
    switch ( hardwareVariant )
    {
        /* Legacy bootloader devices that supports MSFT Extension */
        case kBluetoothIntelHardwareVariantJfP:
        case kBluetoothIntelHardwareVariantThP:
        case kBluetoothIntelHardwareVariantHrP:
        case kBluetoothIntelHardwareVariantCcP:
        /* All Intel new genration controllers support the Microsoft vendor
         * extension are using 0xFC1E for VsMsftOpCode.
         */
        case kBluetoothIntelHardwareVariantTyP:
        case kBluetoothIntelHardwareVariantSlr:
        case kBluetoothIntelHardwareVariantSlrF:
            mMicrosoftExtensionOpCode = 0xFC1E;
            // What operations need to be done?
            break;
        default:
            /* Not supported */
            break;
    }
}

IOReturn IntelBluetoothHostController::ResetToBootloader(bool retry)
{
    IOReturn err;
    BluetoothHCIRequestID id;

    mBootloaderMode = true;
    mDownloading = true;
    mBooting = false;
    
    err = HCIRequestCreate(&id);
    if ( err )
    {
        REQUIRE_NO_ERR(err);
        return err;
    }
    err = BluetoothHCISendIntelReset(id, 0x01, true, true, 0x00, 0x00000000);
    HCIRequestDelete(NULL, id);
    if ( err )
    {
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][ResetToBootloader] -- BluetoothHCISendIntelReset() failed -- cannot deliver Intel reset: 0x%x ****\n", err);
        return err;
    }
    
    if ( retry )
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][ResetToBootloader] -- Reset is sent successfully. Retrying firmware download... ****\n");
    
    /* Current Intel BT controllers(ThP/JfP) hold the USB reset
     * lines for 2ms when it receives Intel Reset in bootloader mode.
     * Whereas, the upcoming Intel BT controllers will hold USB reset
     * for 150ms. To keep the delay generic, 150ms is chosen here.
     */
    IOSleep(150);

    return kIOReturnSuccess;
}

IOReturn IntelBluetoothHostController::BluetoothHCIIntelReadExceptionInfo(BluetoothHCIRequestID inID, BluetoothIntelExceptionInfo * info)
{
    IOReturn err;

    err = PrepareRequestForNewCommand(inID, NULL, 0xFFFF);
    if ( err )
    {
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][BluetoothHCIIntelReadExceptionInfo] -- Failed to prepare request for new command: 0x%x ****\n", err);
        return err;
    }

    err = SendHCIRequestFormatted(inID, 0xFC22, sizeof(BluetoothIntelExceptionInfo), info, "Hbb", 0xFC22, 1, 0x00);
    if ( err )
    {
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][BluetoothHCIIntelReadExceptionInfo] ### ERROR: opCode = 0x%04X -- send request failed -- Unable to obtain exception info: 0x%x ****\n", 0xFC22, err);
        return err;
    }

    os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][BluetoothHCIIntelReadExceptionInfo] -- Exception Info: %s ****\n", info->info);

    return kIOReturnSuccess;
}

IOReturn IntelBluetoothHostController::WriteDeviceAddress(BluetoothHCIRequestID inID, BluetoothDeviceAddress * inAddress)
{
    IOReturn err;
    
    err = PrepareRequestForNewCommand(inID, NULL, 0xFFFF);
    if ( err )
    {
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][WriteDeviceAddress] -- Failed to prepare request for new command: 0x%x ****\n", err);
        return err;
    }
    
    err = SendHCIRequestFormatted(inID, 0xFC31, 0, NULL, "Hb^", 0xFC31, 6, inAddress);
    if ( err )
    {
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][WriteDeviceAddress] ### ERROR: opCode = 0x%04X -- send request failed: 0x%x ****\n", 0xFC31, err);
        return err;
    }
    
    return kIOReturnSuccess;
}

IOReturn IntelBluetoothHostController::CheckDeviceAddress()
{
    IOReturn err;
    BluetoothDeviceAddress address;
    BluetoothDeviceAddress defaultAddress = (BluetoothDeviceAddress) {0x00, 0x8B, 0x9E, 0x19, 0x03, 0x00};
    BluetoothHCIRequestID id;

    err = HCIRequestCreate(&id);
    if ( err )
    {
        REQUIRE_NO_ERR(err);
        return err;
    }
    err = BluetoothHCIReadDeviceAddress(id, &address);
    HCIRequestDelete(NULL, id);
    if ( err )
    {
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][CheckDeviceAddress] -- BluetoothHCIReadDeviceAddress() failed -- cannot read device address: 0x%x ****\n", err);
        return err;
    }

    os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][CheckDeviceAddress] -- Printing device address: 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x ****\n", address.data[0], address.data[1], address.data[2], address.data[3], address.data[4], address.data[5]);

    /* For some Intel based controllers, the default Bluetooth device
     * address 00:03:19:9E:8B:00 can be found. These controllers are
     * fully operational, but have the danger of duplicate addresses
     * and that in turn can cause problems with Bluetooth operation.
     */
    if ( !memcmp(&address, &defaultAddress, 6) )
    {
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][CheckDeviceAddress] -- Default device address (%pMR) found! ****\n", &address);
        mInvalidDeviceAddress = true;
    }

    return kIOReturnSuccess;
}

IOReturn IntelBluetoothHostController::CallBluetoothHCIIntelReadVersionInfo(UInt8 param)
{
    IOReturn err;
    BluetoothHCIRequestID id;

    err = HCIRequestCreate(&id);
    if ( err )
    {
        REQUIRE_NO_ERR(err);
        return err;
    }
    err = BluetoothHCIIntelReadVersionInfo(id, param, mVersionInfo);
    if ( err )
    {
        REQUIRE_NO_ERR(err);
        return err;
    }
    HCIRequestDelete(NULL, id);

    if ( !mVersionInfo )
        return kIOReturnInvalid;

    if ( *(UInt8 *) mVersionInfo )
        return *(UInt8 *) mVersionInfo;

    return err;
}

const char * IntelBluetoothHostController::ConvertFirmwareVariantToString(BluetoothHCIIntelFirmwareVariant firmwareVariant)
{
    switch ( firmwareVariant )
    {
        case kBluetoothHCIIntelFirmwareVariantLegacyROM2_5:
            return "Legacy ROM 2.5";
            
        case kBluetoothHCIIntelFirmwareVariantBootloader:
            return "Bootloader";
            
        case kBluetoothHCIIntelFirmwareVariantLegacyROM2_X:
            return "Legacy ROM 2.x";
            
        case kBluetoothHCIIntelFirmwareVariantFirmware:
            return "Firmware";
            
        default:
            os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][ConvertFirmwareVariantToString] -- Unsupported firmware variant: %02x ****\n", firmwareVariant);
            return "Unknown";
    }
}

const char * IntelBluetoothHostController::ConvertImageTypeToString(BluetoothHCIIntelImageType imageType)
{
    switch ( imageType )
    {
        case kBluetoothHCIIntelImageTypeBootloader:
            return "Bootloader";
            
        case kBluetoothHCIIntelImageTypeFirmware:
            return "Firmware";
            
        default:
            os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][ConvertImageTypeToString] -- Unsupported image type: %02x ****\n", imageType);
            return "Unknown";
    }
}

IOReturn IntelBluetoothHostController::PrintVersionInfo(BluetoothIntelVersionInfo * version)
{
    /* The hardware platform number has a fixed value of 0x37 and
     * for now only accept this single value.
     */
    if ( version->hardwarePlatform != 0x37 )
    {
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][PrintVersionInfo] -- Unsupported hardware platform: %u ****\n", version->hardwarePlatform);
        return kIOReturnInvalid;
    }

    /* Check for supported iBT hardware variants of this firmware
     * loading method.
     *
     * This check has been put in place to ensure correct forward
     * compatibility options when newer hardware variants come along.
     */
    if ( version->hardwareVariant != kBluetoothIntelHardwareVariantWP
      && version->hardwareVariant != kBluetoothIntelHardwareVariantStP
      && version->hardwareVariant != kBluetoothIntelHardwareVariantSfP
      && version->hardwareVariant != kBluetoothIntelHardwareVariantWsP
      && version->hardwareVariant != kBluetoothIntelHardwareVariantJfP
      && version->hardwareVariant != kBluetoothIntelHardwareVariantThP
      && version->hardwareVariant != kBluetoothIntelHardwareVariantHrP
      && version->hardwareVariant != kBluetoothIntelHardwareVariantCcP )
    {
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][PrintVersionInfo] -- Unsupported hardware variant: %u ****\n", version->hardwareVariant);
        return kIOReturnInvalid;
    }

    os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][PrintVersionInfo] -- %s -- Firmware Revision: %u.%u -- Firmware Build: %u - week: %u - year: %u ****\n", ConvertFirmwareVariantToString(version->firmwareVariant), version->firmwareRevision >> 4, version->firmwareRevision & 0x0F, version->firmwareBuildNum, version->firmwareBuildWeek, 2000 + version->firmwareBuildYear);

    return kIOReturnSuccess;
}

IOReturn IntelBluetoothHostController::PrintVersionInfo(BluetoothIntelVersionInfoTLV * version)
{
    /* The hardware platform number has a fixed value of 0x37 and
     * for now only accept this single value.
     */
    if ( IntelCNVXExtractHardwarePlatform(version->cnviBT) != 0x37 )
    {
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][PrintVersionInfo] -- Unsupported hardware platform: 0x%02x ****\n", IntelCNVXExtractHardwarePlatform(version->cnviBT));
        return kIOReturnInvalid;
    }

    /* Check for supported iBT hardware variants of this firmware
     * loading method.
     *
     * This check has been put in place to ensure correct forward
     * compatibility options when newer hardware variants come along.
     */
    switch ( IntelCNVXExtractHardwareVariant(version->cnviBT) )
    {
        case kBluetoothIntelHardwareVariantTyP:
        case kBluetoothIntelHardwareVariantSlr:
        case kBluetoothIntelHardwareVariantSlrF:
            break;
        default:
            os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][PrintVersionInfo] -- Unsupported hardware variant: 0x%x ****\n", IntelCNVXExtractHardwareVariant(version->cnviBT));
            return kIOReturnInvalid;
    }

    if ( version->imageType == kBluetoothHCIIntelImageTypeBootloader )
    {
        /* It is required that every single firmware fragment is acknowledged
         * with a command complete event. If the boot parameters indicate
         * that this bootloader does not send them, then abort the setup.
         */
        if ( version->limitedCCE != 0x00 )
        {
            os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][PrintVersionInfo] -- Unsupported firmware loading method: 0x%x ****\n", version->limitedCCE);
            return kIOReturnInvalid;
        }

        /* Secure boot engine type should be either 1 (ECDSA) or 0 (RSA) */
        if ( version->sbeType > 0x01 )
        {
            os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][PrintVersionInfo] -- Unsupported secure boot engine type: 0x%x ****\n", version->sbeType);
            return kIOReturnInvalid;
        }

        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][PrintVersionInfo] -- Device revision is %u ****\n", version->deviceRevisionID);
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][PrintVersionInfo] -- Secure boot is %s ****\n", version->secureBoot ? "enabled" : "disabled");
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][PrintVersionInfo] -- OTP lock is %s ****\n", version->otpLock ? "enabled" : "disabled");
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][PrintVersionInfo] -- API lock is %s ****\n", version->apiLock ? "enabled" : "disabled");
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][PrintVersionInfo] -- Debug lock is %s ****\n", version->debugLock ? "enabled" : "disabled");
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][PrintVersionInfo] -- Minimum firmware build %u week %u %u ****\n", version->firmwareBuildNumber, version->firmwareBuildWeek, 2000 + version->firmwareBuildYear);
    }

    os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][PrintVersionInfo] -- %s -- Timestamp: %u.%u -- Build Type: %u -- Build Number: %u ****\n", ConvertImageTypeToString(version->imageType), 2000 + (version->timestamp >> 8), version->timestamp & 0xFF, version->buildType, version->buildNumber);

    return kIOReturnSuccess;
}

IOReturn IntelBluetoothHostController::SetQualityReport(bool enable)
{
    IOReturn err;
    BluetoothHCIRequestID id;
    BluetoothIntelDebugFeatures features;

    os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][SetQualityReport] -- %s quality report... ****\n", enable ? "Setting" : "Resetting");

     /* Read the Intel supported features and if new exception formats
      * supported, need to load the additional DDC config to enable.
      */
    err = HCIRequestCreate(&id);
    if ( err )
    {
        REQUIRE_NO_ERR(err);
        return err;
    }
    err = BluetoothHCIIntelReadDebugFeatures(id, &features);
    HCIRequestDelete(NULL, id);
    if ( err )
        return err;

    /* Set or reset the debug features. */
    if ( enable )
        err = SetDebugFeatures(&features);
    else
        err = ResetDebugFeatures(&features);

    return err;
}

#if 0
static int btintel_get_codec_config_data(struct hci_dev *hdev,
                   __u8 link, struct bt_codec *codec,
                   __u8 *ven_len, __u8 **ven_data)
{
    int err = 0;

    if ( !ven_data || !ven_len )
        return -EINVAL;

    *ven_len = 0;
    *ven_data = NULL;

    if ( link != ESCO_LINK ) {
        bt_dev_err(hdev, "Invalid link type(%u)", link);
        return -EINVAL;
    }

    *ven_data = kmalloc(sizeof(__u8), GFP_KERNEL);
    if ( !*ven_data ) {
        err = -ENOMEM;
        goto error;
    }

    // supports only CVSD and mSBC offload codecs
    switch ( codec->id )
    {
        case 0x02:
            **ven_data = 0x00;
            break;
        case 0x05:
            **ven_data = 0x01;
            break;
        default:
            err = -EINVAL;
            bt_dev_err(hdev, "Invalid codec id(%u)", codec->id);
            goto error;
    }

    /* codec and its capabilities are pre-defined to ids
     * preset id = 0x00 represents CVSD codec with sampling rate 8K
     * preset id = 0x01 represents mSBC codec with sampling rate 16K
     */

    *ven_len = sizeof(__u8);
    return err;

error:
    kfree(*ven_data);
    *ven_data = NULL;
    return err;
}
#endif

IOReturn IntelBluetoothHostController::ConfigureOffload()
{
    IOReturn err;
    BluetoothHCIRequestID id;
    BluetoothIntelOffloadUseCases cases;

    err = HCIRequestCreate(&id);
    if ( err )
    {
        REQUIRE_NO_ERR(err);
        return err;
    }
    err = BluetoothHCIIntelReadOffloadUseCases(id, &cases);
    HCIRequestDelete(NULL, id);

    if ( cases.preset[0] & 0x03 )
    {
        /* Intel uses 1 as data path id for all the usecases */
        //*data_path_id = 1;
        //hdev->get_codec_config_data = btintel_get_codec_config_data;
    }

    return err;
}

IOReturn IntelBluetoothHostController::WaitForFirmwareDownload(UInt32 callTime, UInt32 deadline)
{
    IOReturn err;
    AbsoluteTime duration;

    mFirmwareLoaded = true;
    setProperty("FirmwareLoaded", true);

    os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][WaitForFirmwareDownload] -- Waiting for firmware download to complete... ****\n");

    err = ControllerCommandSleep(&mDownloading, deadline, (char *) __FUNCTION__, true);
    if ( err )
    {
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][WaitForFirmwareDownload] -- Firmware loading timed out! ****\n");
        return kIOReturnTimeout;
    }

    if ( mFirmwareLoadingFailed )
    {
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][WaitForFirmwareDownload] -- Firmware loading failed! ****\n");
        return kIOReturnError;
    }

    absolutetime_to_nanoseconds(mBluetoothFamily->GetCurrentTime() - callTime, &duration);
    os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][WaitForFirmwareDownload] -- Firmware loaded in %llu usecs. ****\n", duration >> 10);

    return kIOReturnSuccess;
}

IOReturn IntelBluetoothHostController::WaitForDeviceBoot(UInt32 callTime, UInt32 deadline)
{
    IOReturn err;
    AbsoluteTime duration;

    err = ControllerCommandSleep(&mBooting, deadline, (char *) __FUNCTION__, true);
    if ( err == THREAD_INTERRUPTED )
    {
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][WaitForDeviceBoot] -- Device boot interrupted! ****\n");
        return THREAD_INTERRUPTED;
    }
    if ( err )
    {
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][WaitForDeviceBoot] -- Device boot timed out! ****\n");
        return kIOReturnTimeout;
    }

    absolutetime_to_nanoseconds(mBluetoothFamily->GetCurrentTime() - callTime, &duration);
    os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][WaitForDeviceBoot] -- Device booted in %llu usecs. ****\n", duration >> 10);

    return kIOReturnSuccess;
}

IOReturn IntelBluetoothHostController::BootDevice(UInt32 bootAddress)
{
    IOReturn err;
    BluetoothHCIRequestID id;

    mBooting = true;

    err = HCIRequestCreate(&id);
    if ( err )
    {
        REQUIRE_NO_ERR(err);
        goto reset;
    }
    err = BluetoothHCISendIntelReset(id, 0, true, false, 1, bootAddress);
    HCIRequestDelete(NULL, id);
    if ( err )
    {
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][BootDevice] -- Soft reset failed: 0x%x ****\n", err);
reset:
        ResetToBootloader(false);
        return err;
    }

    /* The bootloader will not indicate when the device is ready. This
     * is done by the operational firmware sending bootup notification.
     *
     * Booting into operational firmware should not take longer than
     * 1 second. However if that happens, then just fail the setup
     * since something went wrong.
     */
    err = WaitForDeviceBoot(mBluetoothFamily->GetCurrentTime(), 1000);
    if ( err )
        goto reset;

    return kIOReturnSuccess;
}

void IntelBluetoothHostController::ProcessEventDataWL(UInt8 * inDataPtr, UInt32 inDataSize, UInt32 sequenceNumber)
{
    IOReturn err;
    BluetoothHCIRequestID id;
    BluetoothIntelExceptionInfo info;

    if ( inDataSize <= kBluetoothHCIEventPacketHeaderSize )
        return;

    BluetoothHCIEventPacketHeader * event = (BluetoothHCIEventPacketHeader *) inDataPtr;
    if ( event->dataSize != inDataSize - sizeof(BluetoothHCIEventPacketHeader) )
        return;

    if ( event->eventCode == kBluetoothHCIEventHardwareError )
    {
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][ProcessEventDataWL] -- Received hardware error: 0x%02x ****\n", *(UInt8 *) (inDataPtr + kBluetoothHCIEventPacketHeaderSize));

        err = HCIRequestCreate(&id);
        if ( err )
        {
            REQUIRE_NO_ERR(err);
            return;
        }
        err = BluetoothHCIIntelReadExceptionInfo(id, &info);
        if ( err )
            return;
        HCIRequestDelete(NULL, id);
    }

    if ( mBootloaderMode && event->dataSize > 0 && event->eventCode == 0xFF )
    {
        UInt8 * param = inDataPtr + kBluetoothHCIEventPacketHeaderSize + 1;
        UInt32 paramSize = inDataSize - kBluetoothHCIEventPacketHeaderSize - 1;

        switch ( inDataPtr[2] )
        {
            case 0x02:
                /* When switching to the operational firmware
                 * the device sends a vendor specific event
                 * indicating that the bootup completed.
                 */
                if ( paramSize != sizeof(BluetoothIntelBootupEventParams) )
                    return;

                if ( mBooting )
                {
                    mBooting = false;
                    mCommandGate->commandWakeup(&mBooting);
                }
                break;

            case 0x06:
                /* When the firmware loading completes the
                 * device sends out a vendor specific event
                 * indicating the result of the firmware
                 * loading.
                 */
                BluetoothIntelSecureSendResultEventParams * eventParam = (BluetoothIntelSecureSendResultEventParams *) param;

                if ( paramSize != sizeof(BluetoothIntelSecureSendResultEventParams) )
                    return;

                if ( eventParam->result )
                    mFirmwareLoadingFailed = true;

                if ( mDownloading && mFirmwareLoaded )
                {
                    mDownloading = false;
                    mCommandGate->commandWakeup(&mDownloading);
                }
                break;
        }
    }

    super::ProcessEventDataWL(inDataPtr, inDataSize, sequenceNumber);
}

bool IntelBluetoothHostController::SetHCIRequestRequireEvents(BluetoothHCICommandOpCode opCode, IOBluetoothHCIRequest * request)
{
    if ( !request )
        return false;

    switch ( opCode )
    {
        case 0xFC01:
        case 0xFC05:
        case 0xFC09:
        case 0xFC52:
        case 0xFC86:
        case 0xFC8B:
        case 0xFCA1:
        case 0xFCA6:
            request->mExpectedEvent = 2; // command complete
            request->mNumberOfExpectedExplicitCompleteEvents = 0;
            return true;

        default:
            return super::SetHCIRequestRequireEvents(opCode, request);
    }
}

bool IntelBluetoothHostController::GetCompleteCodeForCommand(BluetoothHCICommandOpCode inOpCode, BluetoothHCIEventCode * outEventCode)
{
    BluetoothHCIEventCode eventCode;

    switch ( inOpCode )
    {
        case 0xFC01:
        case 0xFC05:
        case 0xFC09:
        case 0xFC52:
        case 0xFC86:
        case 0xFC8B:
        case 0xFCA1:
        case 0xFCA6:
            eventCode = kBluetoothHCIEventCommandComplete;
            break;

        default:
            return super::GetCompleteCodeForCommand(inOpCode, outEventCode);
    }

    if ( outEventCode )
        *outEventCode = eventCode;

    return true;
}

IOReturn IntelBluetoothHostController::LoadDDCConfig(OSData * fwData)
{
    IOReturn err;
    BluetoothHCIRequestID id;
    UInt8 * fwPtr;
    UInt8 cmdDataSize;

    fwPtr = (UInt8 *) fwData->getBytesNoCopy();

    /* DDC file contains one or more DDC structure which has
     * Length (1 byte), DDC ID (2 bytes), and DDC value (Length - 2).
     */
    while ( fwData->getLength() > fwPtr - (UInt8 *) fwData->getBytesNoCopy() )
    {
        cmdDataSize = fwPtr[0] + sizeof(UInt8);

        err = HCIRequestCreate(&id);
        if ( err )
        {
            REQUIRE_NO_ERR(err);
            return err;
        }
        err = BluetoothHCIIntelWriteDDC(id, fwPtr, cmdDataSize);
        if ( err )
            return err;
        HCIRequestDelete(NULL, id);

        fwPtr += cmdDataSize;
    }

    os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][LoadDDCConfig] -- Successfully applied DDC parameters! ****\n");

    return kIOReturnSuccess;
}

IOReturn IntelBluetoothHostController::BluetoothHCIIntelSecureSend(BluetoothHCIIntelSecureSendFragmentType fragmentType, UInt32 paramSize, const UInt8 * param)
{
    IOReturn err;
    UInt8 fragmentSize;
    BluetoothHCIRequestID id;

    while ( paramSize > 0 )
    {
        fragmentSize = (paramSize > 252) ? 252 : paramSize;

        err = HCIRequestCreate(&id);
        if ( err )
        {
            REQUIRE_NO_ERR(err);
            return err;
        }
        err = PrepareRequestForNewCommand(id, NULL, 0xFFFF);
        if ( err )
        {
            os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][BluetoothHCIIntelSecureSend] -- Failed to prepare request for new command: 0x%x ****\n", err);
            return err;
        }
        err = SendHCIRequestFormatted(id, 0xFC09, 0, NULL, "Hbbn", 0xFC09, fragmentSize + 1, fragmentType, fragmentSize, param);
        HCIRequestDelete(NULL, id);
        if ( err )
        {
            REQUIRE_NO_ERR(err);
            return err;
        }

        paramSize -= fragmentSize;
        param += fragmentSize;
    }

    return kIOReturnSuccess;
}

IOReturn IntelBluetoothHostController::BluetoothHCISendIntelReset(BluetoothHCIRequestID inID, UInt8 resetType, bool enablePatch, bool reloadDDC, UInt8 bootOption, UInt32 bootAddress)
{
    IOReturn err;
    
    err = PrepareRequestForNewCommand(inID, NULL, 0xFFFF);
    if ( err )
    {
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][BluetoothHCISendIntelReset] -- Failed to prepare request for new command: 0x%x ****\n", err);
        return err;
    }
    
    err = SendHCIRequestFormatted(inID, 0xFC01, 0, NULL, "HbbbbbW", 0xFC01, 8, resetType, enablePatch, reloadDDC, bootOption, bootAddress);
    if ( err )
    {
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][BluetoothHCISendIntelReset] ### ERROR: opCode = 0x%04X -- send request failed: 0x%x ****\n", 0xFC01, err);
        return err;
    }
    
    return kIOReturnSuccess;
}

IOReturn IntelBluetoothHostController::BluetoothHCIIntelEnterManufacturerMode(BluetoothHCIRequestID inID)
{
    IOReturn err;
    UInt8 status;
    
    err = PrepareRequestForNewCommand(inID, NULL, 0xFFFF);
    if ( err )
    {
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][BluetoothHCIIntelEnterManufacturerMode] -- Failed to prepare request for new command: 0x%x ****\n", err);
        return err;
    }
    
    err = SendHCIRequestFormatted(inID, 0xFC11, 1, &status, "Hbbb", 0xFC11, 2, 0x01, 0x00);
    if ( err )
    {
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][BluetoothHCIIntelEnterManufacturerMode] ### ERROR: opCode = 0x%04X -- send request failed: 0x%x ****\n", 0xFC11, err);
        return err;
    }
    
    return kIOReturnSuccess;
}

IOReturn IntelBluetoothHostController::BluetoothHCIIntelExitManufacturerMode(BluetoothHCIRequestID inID, BluetoothIntelManufacturingExitResetOption resetOption)
{
    IOReturn err;
    UInt8 status;
    
    err = PrepareRequestForNewCommand(inID, NULL, 0xFFFF);
    if ( err )
    {
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][BluetoothHCIIntelExitManufacturerMode] -- Failed to prepare request for new command: 0x%x ****\n", err);
        return err;
    }

    err = SendHCIRequestFormatted(inID, 0xFC11, 1, &status, "Hbbb", 0xFC11, 2, 0x00, resetOption);
    if ( err )
    {
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][BluetoothHCIIntelExitManufacturerMode] ### ERROR: opCode = 0x%04X -- send request failed: 0x%x ****\n", 0xFC11, err);
        return err;
    }
    
    return kIOReturnSuccess;
}

IOReturn IntelBluetoothHostController::CallBluetoothHCIIntelSetEventMask(bool debug)
{
    IOReturn err;
    BluetoothHCIRequestID id;

    if ( mIsLegacyROMDevice )
    {
        err = HCIRequestCreate(&id);
        if ( err )
        {
            REQUIRE_NO_ERR(err);
            return err;
        }
        err = BluetoothHCIIntelEnterManufacturerMode(id);
        HCIRequestDelete(NULL, id);
        if ( err )
            return err;
    }

    err = HCIRequestCreate(&id);
    if ( err )
    {
        REQUIRE_NO_ERR(err);
        return err;
    }
    err = BluetoothHCIIntelSetEventMask(id, debug);
    HCIRequestDelete(NULL, id);
    if ( err )
        return err;

    if ( mIsLegacyROMDevice )
    {
        err = HCIRequestCreate(&id);
        if ( err )
        {
            REQUIRE_NO_ERR(err);
            return err;
        }
        err = BluetoothHCIIntelExitManufacturerMode(id, kBluetoothIntelManufacturingExitResetOptionsNoReset);
        HCIRequestDelete(NULL, id);
        if ( err )
            return err;
    }

    return kIOReturnSuccess;
}

IOReturn IntelBluetoothHostController::BluetoothHCIIntelSetEventMask(BluetoothHCIRequestID inID, bool debug)
{
    IOReturn err;
    UInt8 status;
    
    err = PrepareRequestForNewCommand(inID, NULL, 0xFFFF);
    if ( err )
    {
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][BluetoothHCIIntelSetEventMask] -- Failed to prepare request for new command: 0x%x ****\n", err);
        return err;
    }
    
    err = SendHCIRequestFormatted(inID, 0xFC52, 1, &status, "Hbbbbbbbbb", 0xFC52, 8, 0x87, debug ? 0x6E : 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
    if ( err )
    {
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][BluetoothHCIIntelSetEventMask] ### ERROR: opCode = 0x%04X -- send request failed: 0x%x ****\n", 0xFC52, err);
        return err;
    }

    return kIOReturnSuccess;
}

IOReturn IntelBluetoothHostController::CallBluetoothHCIIntelSetDiagnosticMode(bool enable)
{
    IOReturn err;
    BluetoothHCIRequestID id;

    /* Legacy ROM device needs to be in the manufacturer mode to apply
     * diagnostic settings.
     */
    if ( mIsLegacyROMDevice ) // This flag is set after reading the Intel version.
    {
        err = HCIRequestCreate(&id);
        if ( err )
        {
            REQUIRE_NO_ERR(err);
            return err;
        }
        err = BluetoothHCIIntelEnterManufacturerMode(id);
        HCIRequestDelete(NULL, id);
        if ( err )
            return err;
    }

    err = HCIRequestCreate(&id);
    if ( err )
    {
        REQUIRE_NO_ERR(err);
        return err;
    }
    err = BluetoothHCIIntelSetDiagnosticMode(id, enable);
    HCIRequestDelete(NULL, id);
    if ( err )
        return err;

    err = HCIRequestCreate(&id);
    if ( err )
    {
        REQUIRE_NO_ERR(err);
        return err;
    }
    BluetoothHCIIntelSetEventMask(id, enable);
    HCIRequestDelete(NULL, id);

    if ( mIsLegacyROMDevice )
    {
        err = HCIRequestCreate(&id);
        if ( err )
        {
            REQUIRE_NO_ERR(err);
            return err;
        }
        err = BluetoothHCIIntelExitManufacturerMode(id, kBluetoothIntelManufacturingExitResetOptionsNoReset);
        HCIRequestDelete(NULL, id);
        if ( err )
            return err;
    }

    return kIOReturnSuccess;
}

IOReturn IntelBluetoothHostController::BluetoothHCIIntelSetDiagnosticMode(BluetoothHCIRequestID inID, bool enable)
{
    IOReturn err;
    UInt8 status;

    err = PrepareRequestForNewCommand(inID, NULL, 0xFFFF);
    if ( err )
    {
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][BluetoothHCIIntelSetDiagnosticMode] -- Failed to prepare request for new command: 0x%x ****\n", err);
        return err;
    }

    if ( enable )
        err = SendHCIRequestFormatted(inID, 0xFC43, 1, &status, "Hbbbb", 0xFC43, 3, 0x03, 0x03, 0x03);
    else
        err = SendHCIRequestFormatted(inID, 0xFC43, 1, &status, "Hbbbb", 0xFC43, 3, 0x00, 0x00, 0x00);

    if ( err ) // && err != -ENODATA
    {
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][BluetoothHCIIntelSetDiagnosticMode] ### ERROR: opCode = 0x%04X -- send request failed: 0x%x ****\n", 0xFC43, err);
        return err;
    }

    return kIOReturnSuccess;
}

IOReturn IntelBluetoothHostController::BluetoothHCIIntelReadBootParams(BluetoothHCIRequestID inID, BluetoothIntelBootParams * params)
{
    IOReturn err;

    err = PrepareRequestForNewCommand(inID, NULL, 0xFFFF);
    if ( err )
    {
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][BluetoothHCIIntelReadBootParams] -- Failed to prepare request for new command: 0x%x ****\n", err);
        return err;
    }

    err = SendHCIRequestFormatted(inID, 0xFC0D, sizeof(BluetoothIntelBootParams), params, "Hb", 0xFC0D, 0);
    if ( err )
    {
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][BluetoothHCIIntelReadBootParams] ### ERROR: opCode = 0x%04X -- send request failed: 0x%x ****\n", 0xFC0D, err);
        return err;
    }

    os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][BluetoothHCIIntelReadBootParams] -- Device Revision: %u -- Secure Boot: %s -- OTP Lock: %s -- API Lock: %s -- Debug Lock: %s -- Minimum Firmware Build Number: %u -- Week: %u -- Year: %u ****\n", params->deviceRevisionID, params->secureBoot ? "Enabled" : "Disabled", params->otpLock ? "Enabled" : "Disabled", params->apiLock ? "Enabled" : "Disabled", params->debugLock ? "Enabled" : "Disabled", params->minFirmwareBuildNumber, params->minFirmwareBuildWeek, 2000 + params->minFirmwareBuildYear);

    return kIOReturnSuccess;
}

IOReturn IntelBluetoothHostController::BluetoothHCIIntelReadVersionInfo(BluetoothHCIRequestID inID, UInt8 param, UInt8 * response)
{
    IOReturn err;

    if ( !response )
        return kIOReturnInvalid;

    err = PrepareRequestForNewCommand(inID, NULL, 0xFFFF);
    if ( err )
    {
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][BluetoothHCIIntelReadVersionInfo] -- Failed to prepare request for new command: 0x%x ****\n", err);
        return err;
    }

    if ( param == 0x00 )
        err = SendHCIRequestFormatted(inID, 0xFC05, sizeof(BluetoothIntelVersionInfo), response, "Hb", 0xFC05, 0);
    else if ( param == 0xFF )
        err = SendHCIRequestFormatted(inID, 0xFC05, kMaxHCIBufferLength * 4, response, "Hbb", 0xFC05, 1, 0xFF);
    else
    {
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][BluetoothHCIIntelReadVersionInfo] -- Invalid parameter (0x%02X), should be either 0x00 or 0xFF. ****\n", param);
        return kIOReturnInvalid;
    }

    if ( err )
    {
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][BluetoothHCIIntelReadVersionInfo] ### ERROR: opCode = 0x%04X -- send request failed: 0x%x ****\n", 0xFC05, err);
        return err;
    }

    return kIOReturnSuccess;
}

IOReturn IntelBluetoothHostController::BluetoothHCIIntelReadDebugFeatures(BluetoothHCIRequestID inID, BluetoothIntelDebugFeatures * features)
{
    IOReturn err;
    UInt8 * response;

    err = PrepareRequestForNewCommand(inID, NULL, 0xFFFF);
    if ( err )
    {
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][BluetoothHCIIntelReadDebugFeatures] -- Failed to prepare request for new command: 0x%x ****\n", err);
        return err;
    }

    /* Intel controller supports two pages, each page is of 128-bit
     * feature bit mask. And each bit defines specific feature support
     */
    response = IONewZero(UInt8, sizeof(features->page1) + 3);
    err = SendHCIRequestFormatted(inID, 0xFCA6, sizeof(features->page1) + 3, response, "Hbb", 0xFCA6, 1, 1);
    if ( err )
    {
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][BluetoothHCIIntelReadDebugFeatures] ### ERROR: opCode = 0x%04X -- send request failed -- failed to read supported features for page 1: 0x%x ****\n", 0xFCA6, err);
        return err;
    }

    memcpy(features->page1, response + 3, sizeof(features->page1));
    IOSafeDeleteNULL(response, UInt8, sizeof(features->page1) + 3);

    return kIOReturnSuccess;
}

IOReturn IntelBluetoothHostController::SetDebugFeatures(const BluetoothIntelDebugFeatures * features)
{
    IOReturn err;
    BluetoothHCIRequestID id;
    UInt8 mask[11] = { 0x0a, 0x92, 0x02, 0x7f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    UInt8 period[5] = { 0x04, 0x91, 0x02, 0x05, 0x00 };

    if ( !features )
    {
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][SetDebugFeatures] -- Debug features are not read! ****\n");
        return kIOReturnInvalid;
    }

    if ( !(features->page1[0] & 0x3F) )
    {
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][SetDebugFeatures] -- Telemetry exception format not supported. ****\n");
        return kIOReturnSuccess;
    }

    err = HCIRequestCreate(&id);
    if ( err )
    {
        REQUIRE_NO_ERR(err);
        return err;
    }
    err = BluetoothHCIIntelWriteDDC(id, mask, 11);
    HCIRequestDelete(NULL, id);
    if ( err )
    {
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][SetDebugFeatures] -- BluetoothHCIIntelWriteDDC() failed -- telemetry DDC event mask not set: 0x%x ****\n", err);
        return err;
    }

    err = HCIRequestCreate(&id);
    if ( err )
    {
        REQUIRE_NO_ERR(err);
        return err;
    }
    err = BluetoothHCIIntelWriteDDC(id, period, 5);
    HCIRequestDelete(NULL, id);
    if ( err )
    {
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][SetDebugFeatures] -- BluetoothHCIIntelWriteDDC() failed -- periodicity for link statistics traces not set: 0x%x ****\n", err);
        return err;
    }

    err = HCIRequestCreate(&id);
    if ( err )
    {
        REQUIRE_NO_ERR(err);
        return err;
    }
    err = BluetoothHCIIntelSetLinkStatisticsEventsTracing(id, 0x02);
    HCIRequestDelete(NULL, id);
    if ( err )
        return err;

    os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][SetDebugFeatures] -- Set debug features successfully: traceEnable = 0x%02x, mask = 0x%02x ****\n", 0x02, 0x7f);

    return kIOReturnSuccess;
}

IOReturn IntelBluetoothHostController::ResetDebugFeatures(const BluetoothIntelDebugFeatures * features)
{
    IOReturn err;
    BluetoothHCIRequestID id;
    UInt8 mask[11] = { 0x0a, 0x92, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

    if ( !features )
    {
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][ResetDebugFeatures] -- Debug features are not read! ****\n");
        return kIOReturnInvalid;
    }

    if ( !(features->page1[0] & 0x3F) )
    {
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][ResetDebugFeatures] -- Telemetry exception format not supported. ****\n");
        return kIOReturnSuccess;
    }

    err = HCIRequestCreate(&id);
    if ( err )
    {
        REQUIRE_NO_ERR(err);
        return err;
    }

    err = BluetoothHCIIntelSetLinkStatisticsEventsTracing(id, 0x00);
    HCIRequestDelete(NULL, id);
    if ( err )
        return err;

    err = HCIRequestCreate(&id);
    if ( err )
    {
        REQUIRE_NO_ERR(err);
        return err;
    }

    err = BluetoothHCIIntelWriteDDC(id, mask, 11);
    HCIRequestDelete(NULL, id);
    if ( err )
    {
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][ResetDebugFeatures] -- BluetoothHCIIntelWriteDDC() failed -- telemetry DDC event mask not set: 0x%x ****\n", err);
        return err;
    }

    os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][ResetDebugFeatures] -- Reset debug features successfully: traceEnable = 0x%02x, mask = 0x%02x ****\n", 0x00, 0x00);

    return kIOReturnSuccess;
}

IOReturn IntelBluetoothHostController::BluetoothHCIIntelTurnOffDeviceLED(BluetoothHCIRequestID inID)
{
    IOReturn err;
    UInt8 status;

    err = PrepareRequestForNewCommand(inID, NULL, 0xFFFF);
    if ( err )
    {
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][BluetoothHCIIntelTurnOffDeviceLED] -- Failed to prepare request for new command: 0x%x ****\n", err);
        return err;
    }

    err = SendHCIRequestFormatted(inID, 0xFC3F, 1, &status, "Hb", 0xFC3F, 0);
    if ( err )
    {
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][BluetoothHCIIntelTurnOffDeviceLED] ### ERROR: opCode = 0x%04X -- send request failed -- failed to turn off device LED: 0x%x ****\n", 0xFC3F, err);
        return err;
    }

    return kIOReturnSuccess;
}

IOReturn IntelBluetoothHostController::BluetoothHCIIntelWriteDDC(BluetoothHCIRequestID inID, UInt8 * data, UInt8 dataSize)
{
    IOReturn err;

    err = PrepareRequestForNewCommand(inID, NULL, 0xFFFF);
    if ( err )
    {
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][BluetoothHCIIntelWriteDDC] -- Failed to prepare request for new command: 0x%x ****\n", err);
        return err;
    }

    err = SendHCIRequestFormatted(inID, 0xFC8B, 0, NULL, "Hbn", 0xFC8B, dataSize, dataSize, data); // we don't need the response for the Intel_Write_DDC command
    if ( err )
    {
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][BluetoothHCIIntelWriteDDC] ### ERROR: opCode = 0x%04X -- send request failed: 0x%x ****\n", 0xFC8B, err);
        return err;
    }
    
    return kIOReturnSuccess;
}

IOReturn IntelBluetoothHostController::BluetoothHCIIntelReadOffloadUseCases(BluetoothHCIRequestID inID, BluetoothIntelOffloadUseCases * cases)
{
    IOReturn err;

    err = PrepareRequestForNewCommand(inID, NULL, 0xFFFF);
    if ( err )
    {
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][BluetoothHCIIntelReadOffloadUseCases] -- Failed to prepare request for new command: 0x%x ****\n", err);
        return err;
    }

    err = SendHCIRequestFormatted(inID, 0xFC86, sizeof(BluetoothIntelOffloadUseCases), cases, "Hb", 0xFC86, 0);
    if ( err )
    {
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][BluetoothHCIIntelReadOffloadUseCases] ### ERROR: opCode = 0x%04X -- send request failed -- failed to read offload use cases: 0x%x ****\n", 0xFC86, err);
        return err;
    }

    return kIOReturnSuccess;
}

IOReturn IntelBluetoothHostController::BluetoothHCIIntelSetLinkStatisticsEventsTracing(BluetoothHCIRequestID inID, UInt8 param)
{
    IOReturn err;
    UInt8 status;

    err = PrepareRequestForNewCommand(inID, NULL, 0xFFFF);
    if ( err )
    {
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][BluetoothHCIIntelSetLinkStatisticsEventsTracing] -- Failed to prepare request for new command: 0x%x ****\n", err);
        return err;
    }

    err = SendHCIRequestFormatted(inID, 0xFCA1, 1, &status, "Hbb", 0xFCA1, 1, param);
    if ( err )
    {
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][BluetoothHCIIntelSetLinkStatisticsEventsTracing] ### ERROR: opCode = 0x%04X -- send request failed -- failed to %s tracing of link statistics events: 0x%x ****\n", 0xFCA1, param ? "enable" : "disable", err);
        return err;
    }

    return kIOReturnSuccess;
}

IOReturn IntelBluetoothHostController::DownloadFirmwarePayload(OSData * fwData, size_t offset)
{
    IOReturn err;
    BluetoothHCICommandPacket cmd;
    UInt8 * fwPtr;
    UInt32 fragmentSize;

    fwPtr = (UInt8 *) fwData->getBytesNoCopy() + offset;
    fragmentSize = 0;
    err = kIOReturnInvalid;

    while ( fwPtr - (UInt8 *) fwData->getBytesNoCopy() < fwData->getLength() )
    {
        cmd.opCode   = *(BluetoothHCICommandOpCode *)(fwPtr + fragmentSize);
        cmd.dataSize = *(UInt8 *)(fwPtr + fragmentSize + sizeof(BluetoothHCICommandOpCode));

        fragmentSize += (kBluetoothHCICommandPacketHeaderSize + cmd.dataSize);

        /* The parameter length of the secure send command requires
         * a 4 byte alignment. It happens so that the firmware file
         * contains proper Intel_NOP commands to align the fragments
         * as needed.
         *
         * Send set of commands with 4 byte alignment from the
         * firmware data buffer as a single Data fragement.
         */
        if ( !(fragmentSize % 4) )
        {
            err = BluetoothHCIIntelSecureSend(kBluetoothHCIIntelFirmwareFragmentTypeData, fragmentSize, fwPtr);
            if ( err )
            {
                os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][DownloadFirmwarePayload] -- BluetoothHCIIntelSecureSend() failed -- cannot send firmware data: 0x%x ****\n", err);
                return err;
            }

            fwPtr += fragmentSize;
            fragmentSize = 0;
        }
    }

    return err;
}

IOReturn IntelBluetoothHostController::SecureSendSFIRSAFirmwareHeader(OSData * fwData)
{
    IOReturn err;

    /* Start the firmware download transaction with the Init fragment
     * represented by the 128 bytes of CSS header.
     */

    err = BluetoothHCIIntelSecureSend(kBluetoothHCIIntelFirmwareFragmentTypeInit, 128, (UInt8 *) fwData->getBytesNoCopy());
    if ( err )
    {
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][SecureSendSFIRSAFirmwareHeader] -- Failed to send firmware header: %d ****\n", err);
        return err;
    }

    /* Send the 256 bytes of public key information from the firmware
     * as the PKey fragment.
     */
    err = BluetoothHCIIntelSecureSend(kBluetoothHCIIntelFirmwareFragmentTypePKey, 256, (UInt8 *) fwData->getBytesNoCopy() + 128);
    if ( err )
    {
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][SecureSendSFIRSAFirmwareHeader] -- Failed to send firmware PKey: %d ****\n", err);
        return err;
    }

    /* Send the 256 bytes of signature information from the firmware
     * as the Sign fragment.
     */
    err = BluetoothHCIIntelSecureSend(kBluetoothHCIIntelFirmwareFragmentTypeSign, 256, (UInt8 *) fwData->getBytesNoCopy() + 388);
    if ( err )
    {
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][SecureSendSFIRSAFirmwareHeader] -- Failed to send firmware signature: %d ****\n", err);
        return err;
    }

    return kIOReturnSuccess;
}

IOReturn IntelBluetoothHostController::SecureSendSFIECDSAFirmwareHeader(OSData * fwData)
{
    IOReturn err;

    /* Start the firmware download transaction with the Init fragment
     * represented by the 128 bytes of CSS header.
     */
    err = BluetoothHCIIntelSecureSend(kBluetoothHCIIntelFirmwareFragmentTypeInit, 128, (UInt8 *) fwData->getBytesNoCopy() + 644);
    if ( err )
    {
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][SecureSendSFIECDSAFirmwareHeader] -- Failed to send firmware header: %d ****\n", err);
        return err;
    }

    /* Send the 96 bytes of public key information from the firmware
     * as the PKey fragment.
     */
    err = BluetoothHCIIntelSecureSend(kBluetoothHCIIntelFirmwareFragmentTypePKey, 96, (UInt8 *) fwData->getBytesNoCopy() + 644 + 128);
    if ( err )
    {
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][SecureSendSFIECDSAFirmwareHeader] -- Failed to send firmware PKey: %d ****\n", err);
        return err;
    }

    /* Send the 96 bytes of signature information from the firmware
     * as the Sign fragment
     */
    err = BluetoothHCIIntelSecureSend(kBluetoothHCIIntelFirmwareFragmentTypeSign, 96, (UInt8 *) fwData->getBytesNoCopy() + 644 + 224);
    if ( err )
    {
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][SecureSendSFIECDSAFirmwareHeader] -- Failed to send firmware signature: %d ****\n", err);
        return err;
    }
    return kIOReturnSuccess;
}

bool IntelBluetoothHostController::CheckFirmwareVersion(UInt8 number, UInt8 week, UInt8 year, OSData * fwData, UInt32 * bootAddress)
{
    UInt8 * fwPtr = (UInt8 *) fwData->getBytesNoCopy();
    
    while ( (fwPtr - (UInt8 *) fwData->getBytesNoCopy()) < fwData->getLength() )
    {
        /* Each SKU has a different reset parameter to use in the
         * HCI_Intel_Reset command and it is embedded in the firmware
         * data. So, instead of using static value per SKU, check
         * the firmware data and save it for later use.
         */
        if ( *(BluetoothHCICommandOpCode *) fwPtr == BluetoothHCIMakeCommandOpCode(kBluetoothHCICommandGroupVendorSpecific, kBluetoothHCIIntelCommandWriteBootParams) )
        {
            BluetoothIntelCommandWriteBootParams * params = (BluetoothIntelCommandWriteBootParams *) (fwPtr + kBluetoothHCICommandPacketHeaderSize);

            *bootAddress = params->bootAddress;
            os_log(mInternalOSLogObject, "**** [IntelBluetoothHostController][CheckFirmwareVersion] -- Boot Address: 0x%x -- Firmware Version: %u-%u.%u ****\n", *bootAddress, params->firmwareBuildNumber, params->firmwareBuildWeek, params->firmwareBuildYear);

            return (number == params->firmwareBuildNumber && week == params->firmwareBuildWeek && year == params->firmwareBuildYear);
        }
        
        fwPtr += (kBluetoothHCICommandPacketHeaderSize + *(UInt8 *) (fwPtr + sizeof(BluetoothHCICommandOpCode)));
    }

    return false;
}

OSMetaClassDefineReservedUnused(IntelBluetoothHostController, 0)
OSMetaClassDefineReservedUnused(IntelBluetoothHostController, 1)
OSMetaClassDefineReservedUnused(IntelBluetoothHostController, 2)
OSMetaClassDefineReservedUnused(IntelBluetoothHostController, 3)
OSMetaClassDefineReservedUnused(IntelBluetoothHostController, 4)
OSMetaClassDefineReservedUnused(IntelBluetoothHostController, 5)
OSMetaClassDefineReservedUnused(IntelBluetoothHostController, 6)
OSMetaClassDefineReservedUnused(IntelBluetoothHostController, 7)
OSMetaClassDefineReservedUnused(IntelBluetoothHostController, 8)
OSMetaClassDefineReservedUnused(IntelBluetoothHostController, 9)
OSMetaClassDefineReservedUnused(IntelBluetoothHostController, 10)
OSMetaClassDefineReservedUnused(IntelBluetoothHostController, 11)
OSMetaClassDefineReservedUnused(IntelBluetoothHostController, 12)
OSMetaClassDefineReservedUnused(IntelBluetoothHostController, 13)
OSMetaClassDefineReservedUnused(IntelBluetoothHostController, 14)
OSMetaClassDefineReservedUnused(IntelBluetoothHostController, 15)
OSMetaClassDefineReservedUnused(IntelBluetoothHostController, 16)
OSMetaClassDefineReservedUnused(IntelBluetoothHostController, 17)
OSMetaClassDefineReservedUnused(IntelBluetoothHostController, 18)
OSMetaClassDefineReservedUnused(IntelBluetoothHostController, 19)
OSMetaClassDefineReservedUnused(IntelBluetoothHostController, 20)
OSMetaClassDefineReservedUnused(IntelBluetoothHostController, 21)
OSMetaClassDefineReservedUnused(IntelBluetoothHostController, 22)
OSMetaClassDefineReservedUnused(IntelBluetoothHostController, 23)
