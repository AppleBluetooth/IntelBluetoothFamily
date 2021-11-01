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

#include "IntelBluetoothHostController.h"

#define super IOBluetoothHostController
OSDefineMetaClassAndAbstractStructors(IntelBluetoothHostController, super)

bool IntelBluetoothHostController::init( IOBluetoothHCIController * family, IOBluetoothHostControllerTransport * transport )
{
    if (!super::init(family, transport))
        return false;
    mExpansionData = IONewZero(ExpansionData, 1);
    if (!mExpansionData)
        return false;
    mVersionInfo = NULL;
    return true;
}

void IntelBluetoothHostController::free()
{
    mVersionInfo = NULL;
    IODelete(mExpansionData, ExpansionData, 1);
    super::free();
}

bool IntelBluetoothHostController::start(IOService * provider)
{
    if (!super::start(provider))
        return false;
    
    IOReturn err;
    
    /* The some controllers have a bug with the first HCI command sent to it
     * returning number of completed commands as zero. This would stall the
     * command processing in the Bluetooth core.
     *
     * As a workaround, send HCI Reset command first which will reset the
     * number of completed commands and allow normal command processing
     * from now on.
     */
    
    if (mExpansionData->mBrokenInitialNumberOfCommands)
    {
        err = CallBluetoothHCIReset(false, (char *) __FUNCTION__);
        if (err)
            return false;
    }

    /* Starting from TyP device, the command parameter and response are
     * changed even though the OCF for HCI_Intel_Read_Version command
     * remains same. The legacy devices can handle even if the
     * command has a parameter and returns a correct version information.
     * So, it uses new format to support both legacy and new format.
     */
    
    err = CallBluetoothHCIIntelReadVersionInfo(0xFF);
    if (err)
        return false;

    /* Apply the common HCI quirks for Intel device */
    mExpansionData->mStrictDuplicateFilter = true;
    mExpansionData->mSimultaneousDiscovery = true;
    mExpansionData->mDiagnosticModeNotPersistent = true;
    
    return true;
}

void IntelBluetoothHostController::stop(IOService * provider)
{
    IOReturn err;
    BluetoothHCIRequestID id;
    
    super::stop(provider);
    
    /* Send HCI Reset to the controller to stop any BT activity which
     * were triggered. This will help to save power and maintain the
     * sync b/w Host and controller
     */
    err = CallBluetoothHCIReset(false, (char *) __FUNCTION__);
    if ( err )
        return;
    
    /* Some platforms have an issue with BT LED when the interface is
     * down or BT radio is turned off, which takes 5 seconds to BT LED
     * goes off. This command turns off the BT LED immediately.
     */
    if ( mExpansionData->mBrokenLED )
    {
        HCIRequestCreate(&id);
        BluetoothHCIIntelTurnOffDeviceLED(id);
        HCIRequestDelete(NULL, id);
    }
}

void IntelBluetoothHostController::SetMicrosoftExtensionOpCode(UInt8 hardwareVariant)
{
    switch (hardwareVariant)
    {
        /* Legacy bootloader devices that supports MSFT Extension */
        case 0x11:    /* JfP */
        case 0x12:    /* ThP */
        case 0x13:    /* HrP */
        case 0x14:    /* CcP */
        /* All Intel new genration controllers support the Microsoft vendor
         * extension are using 0xFC1E for VsMsftOpCode.
         */
        case 0x17:
        case 0x18:
        case 0x19:
            mMicrosoftExtensionOpCode = 0xFC1E;
            // What operations need to be done?
            break;
        default:
            /* Not supported */
            break;
    }
}

void IntelBluetoothHostController::ResetToBootloader(BluetoothHCIRequestID inID)
{
    IOReturn err;

    err = BluetoothHCISendIntelReset(inID, 0x01, true, true, 0x00, 0x00000000);
    if (err)
    {
        os_log(mInternalOSLogObject, "[IntelBluetoothHostController][ResetToBootloader] BluetoothHCISendIntelReset() failed -- cannot deliver Intel reset: 0x%x", err);
        return;
    }
    
    os_log(mInternalOSLogObject, "[IntelBluetoothHostController][ResetToBootloader] Reset is sent successfully. Retrying firmware download...");
    
    /* Current Intel BT controllers(ThP/JfP) hold the USB reset
     * lines for 2ms when it receives Intel Reset in bootloader mode.
     * Whereas, the upcoming Intel BT controllers will hold USB reset
     * for 150ms. To keep the delay generic, 150ms is chosen here.
     */
    IOSleep(150);
}

void IntelBluetoothHostController::HandleHardwareError(BluetoothHCIRequestID inID, UInt8 code)
{
    IOReturn err;
    char * buffer;
    
    os_log(mInternalOSLogObject, "[IntelBluetoothHostController][HandleHardwareError] Hardware error: 0x%2.2x", code);

    err = CallBluetoothHCIReset(false, (char *) __FUNCTION__);
    if ( err )
        return;
    
    err = PrepareRequestForNewCommand(inID, NULL, 0xFFFF);
    if ( err )
    {
        os_log(mInternalOSLogObject, "[IntelBluetoothHostController][HandleHardwareError] Failed to prepare request for new command: 0x%x", err);
        return;
    }

    buffer = IONewZero(char, 12);
    err = SendHCIRequestFormatted(inID, 0xFC22, 12, buffer, "Hbb", 0xFC22, 1, 0x00);
    if ( err )
    {
        os_log(mInternalOSLogObject, "[IntelBluetoothHostController][HandleHardwareError] ### ERROR: opCode = 0x%04X -- send request failed -- Unable to obtain exception info: 0x%x", 0xFC22, err);
        return;
    }

    os_log(mInternalOSLogObject, "[IntelBluetoothHostController][HandleHardwareError] Exception Info: %s", buffer);
    IOSafeDeleteNULL(buffer, UInt8, 12);
}

IOReturn IntelBluetoothHostController::WriteDeviceAddress(BluetoothHCIRequestID inID, BluetoothDeviceAddress * inAddress)
{
    IOReturn err;
    
    err = PrepareRequestForNewCommand(inID, NULL, 0xFFFF);
    if ( err )
    {
        os_log(mInternalOSLogObject, "[IntelBluetoothHostController][WriteDeviceAddress] Failed to prepare request for new command: 0x%x", err);
        return err;
    }
    
    err = SendHCIRequestFormatted(inID, 0xFC31, 0, NULL, "Hb^", 0xFC31, 6, inAddress);
    if ( err )
    {
        os_log(mInternalOSLogObject, "[IntelBluetoothHostController][WriteDeviceAddress] ### ERROR: opCode = 0x%04X -- send request failed: 0x%x", 0xFC31, err);
        return err;
    }
    
    return kIOReturnSuccess;
}

IOReturn IntelBluetoothHostController::CheckDeviceAddress(BluetoothHCIRequestID inID)
{
    IOReturn err;
    BluetoothDeviceAddress address;
    BluetoothDeviceAddress defaultAddress = (BluetoothDeviceAddress) {0x00, 0x8b, 0x9e, 0x19, 0x03, 0x00};
    
    err = BluetoothHCIReadDeviceAddress(inID, &address);
    if ( err )
    {
        os_log(mInternalOSLogObject, "[IntelBluetoothHostController][CheckDeviceAddress] BluetoothHCIReadDeviceAddress() failed -- cannot read device address: 0x%x", err);
        return err;
    }

    /* For some Intel based controllers, the default Bluetooth device
     * address 00:03:19:9E:8B:00 can be found. These controllers are
     * fully operational, but have the danger of duplicate addresses
     * and that in turn can cause problems with Bluetooth operation.
     */
    if ( memcmp(&address, &defaultAddress, 6) )
    {
        os_log(mInternalOSLogObject, "[IntelBluetoothHostController][CheckDeviceAddress] Default device address (%pMR) found!", &address);
        mExpansionData->mInvalidDeviceAddress = true;
    }

    return kIOReturnSuccess;
}

IOReturn IntelBluetoothHostController::CallBluetoothHCIIntelReadVersionInfo(UInt8 param)
{
    IOReturn err;
    BluetoothHCIRequestID id;
    
    HCIRequestCreate(&id);
    err = BluetoothHCIIntelReadVersionInfo(id, param, (UInt8 *) mVersionInfo);
    HCIRequestDelete(NULL, id);
    
    return err;
}

IOReturn IntelBluetoothHostController::PrintVersionInfo(BluetoothIntelVersionInfo * version)
{
    const char * variant;

    /* The hardware platform number has a fixed value of 0x37 and
     * for now only accept this single value.
     */
    if (version->hardwarePlatform != 0x37)
    {
        os_log(mInternalOSLogObject, "[IntelBluetoothHostController][PrintVersionInfo] Unsupported hardware platform: %u", version->hardwarePlatform);
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
      && version->hardwareVariant != kBluetoothIntelHardwareVariantCcP ) // there might be a better way to do this - something like if ( !(BluetoothIntelHardwareVariant) version->hardwareVariant )
    {
        os_log(mInternalOSLogObject, "[IntelBluetoothHostController][PrintVersionInfo] Unsupported hardware variant: %u", version->hardwareVariant);
        return kIOReturnInvalid;
    }

    switch (version->firmwareVariant)
    {
        case 0x01:
            variant = "Legacy ROM 2.5";
            break;
        case 0x06:
            variant = "Bootloader";
            break;
        case 0x22:
            variant = "Legacy ROM 2.x";
            break;
        case 0x23:
            variant = "Firmware";
            break;
        default:
            os_log(mInternalOSLogObject, "[IntelBluetoothHostController][PrintVersionInfo] Unsupported firmware variant: %02x", version->firmwareVariant);
            return kIOReturnInvalid;
    }

    os_log(mInternalOSLogObject, "[IntelBluetoothHostController][PrintVersionInfo] Firmware Variant: %s -- Firmware Revision: %u.%u -- Firmware Build: %u - week: %u - year: %u", variant, version->firmwareRevision >> 4, version->firmwareRevision & 0x0f, version->firmwareBuildNum, version->firmwareBuildWeek, 2000 + version->firmwareBuildYear);

    return kIOReturnSuccess;
}

IOReturn IntelBluetoothHostController::PrintVersionInfo(BluetoothIntelVersionInfoTLV * version)
{
    const char * variant;

    /* The hardware platform number has a fixed value of 0x37 and
     * for now only accept this single value.
     */
    if (IntelCNVXExtractHardwarePlatform(version->cnviBT) != 0x37)
    {
        os_log(mInternalOSLogObject, "[IntelBluetoothHostController][PrintVersionInfo] Unsupported hardware platform: 0x%2x", IntelCNVXExtractHardwarePlatform(version->cnviBT));
        return kIOReturnInvalid;
    }

    /* Check for supported iBT hardware variants of this firmware
     * loading method.
     *
     * This check has been put in place to ensure correct forward
     * compatibility options when newer hardware variants come along.
     */
    switch (IntelCNVXExtractHardwareVariant(version->cnviBT))
    {
        case kBluetoothIntelHardwareVariantTyP:
        case kBluetoothIntelHardwareVariantSlr:
        case kBluetoothIntelHardwareVariantSlrF:
            break;
        default:
            os_log(mInternalOSLogObject, "[IntelBluetoothHostController][PrintVersionInfo] Unsupported hardware variant: 0x%x", IntelCNVXExtractHardwareVariant(version->cnviBT));
            return kIOReturnInvalid;
    }

    switch (version->imageType)
    {
        case 0x01:
            variant = "Bootloader";
            /* It is required that every single firmware fragment is acknowledged
             * with a command complete event. If the boot parameters indicate
             * that this bootloader does not send them, then abort the setup.
             */
            if (version->limitedCCE != 0x00)
            {
                os_log(mInternalOSLogObject, "[IntelBluetoothHostController][PrintVersionInfo] Unsupported firmware loading method: 0x%x", version->limitedCCE);
                return kIOReturnInvalid;
            }

            /* Secure boot engine type should be either 1 (ECDSA) or 0 (RSA) */
            if (version->sbeType > 0x01)
            {
                os_log(mInternalOSLogObject, "[IntelBluetoothHostController][PrintVersionInfo] Unsupported secure boot engine type: 0x%x", version->sbeType);
                return kIOReturnInvalid;
            }

            os_log(mInternalOSLogObject, "[IntelBluetoothHostController][PrintVersionInfo] Device revision is %u", version->deviceRevisionID);
            os_log(mInternalOSLogObject, "[IntelBluetoothHostController][PrintVersionInfo] Secure boot is %s", version->secureBoot ? "enabled" : "disabled");
            os_log(mInternalOSLogObject, "[IntelBluetoothHostController][PrintVersionInfo] OTP lock is %s", version->otpLock ? "enabled" : "disabled");
            os_log(mInternalOSLogObject, "[IntelBluetoothHostController][PrintVersionInfo] API lock is %s", version->apiLock ? "enabled" : "disabled");
            os_log(mInternalOSLogObject, "[IntelBluetoothHostController][PrintVersionInfo] Debug lock is %s", version->debugLock ? "enabled" : "disabled");
            os_log(mInternalOSLogObject, "[IntelBluetoothHostController][PrintVersionInfo] Minimum firmware build %u week %u %u", version->firmwareBuildNumber, version->firmwareBuildWeek, 2000 + version->firmwareBuildYear);
            break;
            
        case 0x03:
            variant = "Firmware";
            break;
            
        default:
            os_log(mInternalOSLogObject, "[IntelBluetoothHostController][PrintVersionInfo] Unsupported image type: %02x", version->imageType);
            return kIOReturnInvalid;
    }

    os_log(mInternalOSLogObject, "[IntelBluetoothHostController][PrintVersionInfo] %s timestamp %u.%u buildtype %u build %u", variant, 2000 + (version->timestamp >> 8), version->timestamp & 0xff, version->buildType, version->buildNumber);

    return kIOReturnSuccess;
}

IOReturn IntelBluetoothHostController::WaitForFirmwareDownload(UInt32 callTime, AbsoluteTime deadline)
{
    IOReturn err;
    AbsoluteTime duration;

    mExpansionData->mFirmwareLoaded = true;
    setProperty("FirmwareLoaded", true);

    os_log(mInternalOSLogObject, "[IntelBluetoothHostController][WaitForFirmwareDownload] Waiting for firmware download to complete...");

    err = mCommandGate->commandSleep(&mExpansionData->mDownloading, deadline, THREAD_INTERRUPTIBLE);
    if (err == THREAD_INTERRUPTED)
    {
        os_log(mInternalOSLogObject, "[IntelBluetoothHostController][WaitForFirmwareDownload] Firmware loading is interrupted!");
        return err;
    }

    if (err)
    {
        os_log(mInternalOSLogObject, "[IntelBluetoothHostController][WaitForFirmwareDownload] Firmware loading timed out!");
        return kIOReturnTimeout;
    }

    if (mExpansionData->mFirmwareLoadingFailed)
    {
        os_log(mInternalOSLogObject, "[IntelBluetoothHostController][WaitForFirmwareDownload] Firmware loading failed!");
        return kIOReturnError;
    }
    
    absolutetime_to_nanoseconds(mBluetoothFamily->GetCurrentTime() - callTime, &duration);
    os_log(mInternalOSLogObject, "[IntelBluetoothHostController][WaitForFirmwareDownload] Firmware loaded in %llu usecs.", duration >> 10);

    return kIOReturnSuccess;
}

IOReturn IntelBluetoothHostController::GetFirmwareName(void * version, BluetoothIntelBootParams * params, const char * suffix, char * fwName, IOByteCount size)
{
    return mCommandGate->runAction(GetFirmwareAction, version, params, (void *) suffix, fwName);
}

IOReturn IntelBluetoothHostController::GetFirmwareNameAction(OSObject * owner, void * arg0, void * arg1, void * arg2, void * arg3)
{
    IntelBluetoothHostController * object = OSDynamicCast(IntelBluetoothHostController, owner);
    return object->GetFirmwareNameWL(arg0, (BluetoothIntelBootParams *) arg1, (const char *) arg2, (char *) arg3);
}

IOReturn IntelBluetoothHostController::GetFirmwareNameWL(void * version, BluetoothIntelBootParams * params, const char * suffix, char * fwName)
{
    return kIOReturnUnsupported;
}

IOReturn IntelBluetoothHostController::GetFirmware(void * version, BluetoothIntelBootParams * params, const char * suffix, OSData ** fwData)
{
    return mCommandGate->runAction(GetFirmwareAction, version, params, (void *) suffix, *fwData);
}

IOReturn IntelBluetoothHostController::GetFirmwareAction(OSObject * owner, void * arg0, void * arg1, void * arg2, void * arg3)
{
    IntelBluetoothHostController * object = OSDynamicCast(IntelBluetoothHostController, owner);
    return object->GetFirmwareWL(arg0, (BluetoothIntelBootParams *) arg1, (const char *) arg2, (OSData **) &arg3);
}

IOReturn IntelBluetoothHostController::GetFirmwareWL(void * version, BluetoothIntelBootParams * params, const char * suffix, OSData ** fwData)
{
    return kIOReturnUnsupported;
}

IOReturn IntelBluetoothHostController::DownloadFirmware(BluetoothHCIRequestID inID, void * version, BluetoothIntelBootParams * params, UInt32 * bootAddress)
{
    return mCommandGate->runAction(DownloadFirmwareAction, &inID, version, params, bootAddress);
}

IOReturn IntelBluetoothHostController::DownloadFirmwareAction(OSObject * owner, void * arg0, void * arg1, void * arg2, void * arg3)
{
    IntelBluetoothHostController * object = OSDynamicCast(IntelBluetoothHostController, owner);
    return object->DownloadFirmwareWL(*(BluetoothHCIRequestID *) arg0, arg1, (BluetoothIntelBootParams *) arg2, (UInt32 *) arg3);
}

IOReturn IntelBluetoothHostController::DownloadFirmwareWL(BluetoothHCIRequestID inID, void * version, BluetoothIntelBootParams * params, UInt32 * bootAddress)
{
    return kIOReturnUnsupported;
}

IOReturn IntelBluetoothHostController::LoadDDCConfig(BluetoothHCIRequestID inID, OSData * fwData)
{
    IOReturn err;
    UInt8 * fwPtr;
    UInt8 cmdDataSize;
    
    fwPtr = (UInt8 *) fwData->getBytesNoCopy();

    /* DDC file contains one or more DDC structure which has
     * Length (1 byte), DDC ID (2 bytes), and DDC value (Length - 2).
     */
    while (fwData->getLength() > fwPtr - (UInt8 *) fwData->getBytesNoCopy())
    {
        cmdDataSize = fwPtr[0] + sizeof(UInt8);

        err = BluetoothHCIIntelWriteDDC(inID, fwPtr, cmdDataSize);
        if (err)
            return err;

        fwPtr += cmdDataSize;
    }

    os_log(mInternalOSLogObject, "[IntelBluetoothHostController][LoadDDCConfig] Successfully applied DDC parameters!");

    return kIOReturnSuccess;
}

IOReturn IntelBluetoothHostController::WaitForDeviceBoot(UInt32 callTime, AbsoluteTime deadline)
{
    IOReturn err;
    AbsoluteTime duration;

    os_log(mInternalOSLogObject, "[IntelBluetoothHostController][WaitForDeviceBoot] Waiting for device to boot...");

    err = mCommandGate->commandSleep(&mExpansionData->mBooting, deadline, THREAD_INTERRUPTIBLE);
    if (err == THREAD_INTERRUPTED)
    {
        os_log(mInternalOSLogObject, "[IntelBluetoothHostController][WaitForDeviceBoot] Device boot is interrupted!");
        return err;
    }

    if (err)
    {
        os_log(mInternalOSLogObject, "[IntelBluetoothHostController][WaitForDeviceBoot] Device boot timed out!");
        return kIOReturnTimeout;
    }

    if (mExpansionData->mFirmwareLoadingFailed)
    {
        os_log(mInternalOSLogObject, "[IntelBluetoothHostController][WaitForDeviceBoot] Firmware loading failed!");
        return kIOReturnError;
    }
    
    absolutetime_to_nanoseconds(mBluetoothFamily->GetCurrentTime() - callTime, &duration);
    os_log(mInternalOSLogObject, "[IntelBluetoothHostController][WaitForDeviceBoot] Device booted in %llu usecs.", duration >> 10);

    return kIOReturnSuccess;
}

IOReturn IntelBluetoothHostController::BootDevice(UInt32 bootAddress)
{
    IOReturn err;
    UInt32 callTime;
    BluetoothHCIRequestID id;
    
    callTime = mBluetoothFamily->GetCurrentTime();
    mExpansionData->mBooting = true;

    HCIRequestCreate(&id);
    err = BluetoothHCISendIntelReset(id, 0, true, false, 1, bootAddress);
    HCIRequestDelete(NULL, id);
    if (err)
    {
        os_log(mInternalOSLogObject, "[IntelBluetoothHostController][BootDevice] Soft reset failed: 0x%x", err);

reset:
        HCIRequestCreate(&id);
        ResetToBootloader(id);
        HCIRequestDelete(NULL, id);
        return err;
    }

    /* The bootloader will not indicate when the device is ready. This
     * is done by the operational firmware sending bootup notification.
     *
     * Booting into operational firmware should not take longer than
     * 1 second. However if that happens, then just fail the setup
     * since something went wrong.
     */
    err = WaitForDeviceBoot(callTime, 1000);
    if (err == kIOReturnTimeout)
        goto reset;

    return err;
}

void IntelBluetoothHostController::ProcessEventDataWL(UInt8 * inDataPtr, UInt32 inDataSize, UInt32 sequenceNumber)
{
    super::ProcessEventDataWL(inDataPtr, inDataSize, sequenceNumber);

    if (inDataSize <= kBluetoothHCIEventPacketHeaderSize)
        return;

    BluetoothHCIEventPacketHeader * event = (BluetoothHCIEventPacketHeader *) inDataPtr;
    inDataPtr += sizeof(BluetoothHCIEventPacketHeader);
    inDataSize -= sizeof(BluetoothHCIEventPacketHeader);
    if ( event->dataSize != inDataSize || event->dataSize == 0 )
        return;
    if ( mExpansionData->mBootloaderMode && event->eventCode == 0xFF )
    {
        switch (inDataPtr[0])
        {
            ++inDataPtr;
            --inDataSize;
            case 0x02:
            {
                /* When switching to the operational firmware
                 * the device sends a vendor specific event
                 * indicating that the bootup completed.
                 */
                if (inDataSize != sizeof(BluetoothIntelBootupEventParams))
                    return;

                if ( mExpansionData->mBooting )
                {
                    mExpansionData->mBooting = false;
                    mCommandGate->commandWakeup(&mExpansionData->mBooting);
                }
                break;
            }

            case 0x06:
            {
                /* When the firmware loading completes the
                 * device sends out a vendor specific event
                 * indicating the result of the firmware
                 * loading.
                 */
                BluetoothIntelSecureSendResultEventParams * eventParam = (BluetoothIntelSecureSendResultEventParams *) inDataPtr;

                if (inDataSize != sizeof(BluetoothIntelSecureSendResultEventParams))
                    return;

                if (eventParam->result)
                    mExpansionData->mFirmwareLoadingFailed = true;

                if ( mExpansionData->mDownloading && mExpansionData->mFirmwareLoaded )
                {
                    mExpansionData->mDownloading = false;
                    mCommandGate->commandWakeup(&mExpansionData->mDownloading);
                }
                break;
            }
        }
    }
}

IOReturn IntelBluetoothHostController::HandleSpecialOpcodes(BluetoothHCICommandOpCode opCode)
{
    // Do stuff
    return super::HandleSpecialOpcodes(opCode);
}

IOReturn IntelBluetoothHostController::BluetoothHCIIntelSecureSend(BluetoothHCIRequestID inID, UInt8 fragmentType, UInt32 paramSize, const UInt8 * param)
{
    IOReturn err;
    BluetoothHCICommandPacket packet;
    UInt8 fragmentSize;
    
    while (paramSize > 0)
    {
        fragmentSize = (paramSize > 252) ? 252 : paramSize;

        packet.opCode = 0xFC09;
        packet.dataSize = fragmentSize + 1;
        packet.data[0] = fragmentType;
        memcpy(packet.data + 1, param, fragmentSize);
        
        err = SendRawHCICommand(inID, (char *) &packet, packet.dataSize + kBluetoothHCICommandPacketHeaderSize, NULL, 0);
        if (err)
            return err;

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
        os_log(mInternalOSLogObject, "[IntelBluetoothHostController][BluetoothHCISendIntelReset] Failed to prepare request for new command: 0x%x", err);
        return err;
    }
    
    err = SendHCIRequestFormatted(inID, 0xFC01, 0, NULL, "HbbbbbW", 0xFC01, 8, resetType, enablePatch, reloadDDC, bootOption, bootAddress);
    if (err)
    {
        os_log(mInternalOSLogObject, "[IntelBluetoothHostController][BluetoothHCISendIntelReset] ### ERROR: opCode = 0x%04X -- send request failed: 0x%x", 0xFC01, err);
        return err;
    }
    
    return kIOReturnSuccess;
}

IOReturn IntelBluetoothHostController::BluetoothHCIIntelEnterManufacturerMode(BluetoothHCIRequestID inID)
{
    IOReturn err;
    
    err = PrepareRequestForNewCommand(inID, NULL, 0xFFFF);
    if ( err )
    {
        os_log(mInternalOSLogObject, "[IntelBluetoothHostController][BluetoothHCIIntelEnterManufacturerMode] Failed to prepare request for new command: 0x%x", err);
        return err;
    }
    
    err = SendHCIRequestFormatted(inID, 0xFC11, 0, NULL, "Hbbb", 0xFC11, 2, 0x01, 0x00);
    if ( err )
    {
        os_log(mInternalOSLogObject, "[IntelBluetoothHostController][BluetoothHCIIntelEnterManufacturerMode] ### ERROR: opCode = 0x%04X -- send request failed: 0x%x", 0xFC11, err);
        return err;
    }
    
    return kIOReturnSuccess;
}

IOReturn IntelBluetoothHostController::BluetoothHCIIntelExitManufacturerMode(BluetoothHCIRequestID inID, BluetoothIntelManufacturingExitResetOption resetOption)
{
    IOReturn err;
    
    err = PrepareRequestForNewCommand(inID, NULL, 0xFFFF);
    if ( err )
    {
        os_log(mInternalOSLogObject, "[IntelBluetoothHostController][BluetoothHCIIntelExitManufacturerMode] Failed to prepare request for new command: 0x%x", err);
        return err;
    }

    err = SendHCIRequestFormatted(inID, 0xFC11, 0, NULL, "Hbbb", 0xFC11, 2, 0x00, resetOption);
    if ( err )
    {
        os_log(mInternalOSLogObject, "[IntelBluetoothHostController][BluetoothHCIIntelExitManufacturerMode] ### ERROR: opCode = 0x%04X -- send request failed: 0x%x", 0xFC11, err);
        return err;
    }
    
    return kIOReturnSuccess;
}

IOReturn IntelBluetoothHostController::BluetoothHCIIntelSetEventMask(BluetoothHCIRequestID inID, bool debug)
{
    IOReturn err;
    
    if ( mExpansionData->mIsLegacyROMDevice )
    {
        err = BluetoothHCIIntelEnterManufacturerMode(inID);
        if (err)
            return err;
    }
    
    err = PrepareRequestForNewCommand(inID, NULL, 0xFFFF);
    if ( err )
    {
        os_log(mInternalOSLogObject, "[IntelBluetoothHostController][BluetoothHCIIntelSetEventMask] Failed to prepare request for new command: 0x%x", err);
        return err;
    }
    
    err = SendHCIRequestFormatted(inID, 0xFC52, 0, NULL, "Hbbbbbbbbb", 0xFC52, 8, 0x87, debug ? 0x6E : 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
    if ( err )
    {
        os_log(mInternalOSLogObject, "[IntelBluetoothHostController][BluetoothHCIIntelSetEventMask] ### ERROR: opCode = 0x%04X -- send request failed: 0x%x", 0xFC52, err);
        return err;
    }
    
    if ( mExpansionData->mIsLegacyROMDevice )
    {
        err = BluetoothHCIIntelExitManufacturerMode(inID, kBluetoothIntelManufacturingExitResetOptionsNoReset);
        if (err)
            return err;
    }
    
    return kIOReturnSuccess;
}

IOReturn IntelBluetoothHostController::BluetoothHCIIntelSetDiagnosticMode(BluetoothHCIRequestID inID, bool enable)
{
    IOReturn err;
    
    /* Legacy ROM device needs to be in the manufacturer mode to apply
     * diagnostic settings.
     */
    if ( mExpansionData->mIsLegacyROMDevice ) // This flag is set after reading the Intel version.
    {
        err = BluetoothHCIIntelEnterManufacturerMode(inID);
        if (err)
            return err;
    }
    
    err = PrepareRequestForNewCommand(inID, NULL, 0xFFFF);
    if ( err )
    {
        os_log(mInternalOSLogObject, "[IntelBluetoothHostController][BluetoothHCIIntelSetDiagnosticMode] Failed to prepare request for new command: 0x%x", err);
        return err;
    }
    
    if ( enable )
        err = SendHCIRequestFormatted(inID, 0xFC43, 0, NULL, "Hbbbb", 0xfc43, 3, 0x03, 0x03, 0x03);
    else
        err = SendHCIRequestFormatted(inID, 0xFC43, 0, NULL, "Hbbbb", 0xfc43, 3, 0x00, 0x00, 0x00);
    
    if ( err ) // && err != -ENODATA
    {
        os_log(mInternalOSLogObject, "[IntelBluetoothHostController][BluetoothHCIIntelSetDiagnosticMode] ### ERROR: opCode = 0x%04X -- send request failed: 0x%x", 0xFC43, err);
        return err;
    }
    
    BluetoothHCIIntelSetEventMask(inID, enable);
    
    if ( mExpansionData->mIsLegacyROMDevice )
    {
        err = BluetoothHCIIntelExitManufacturerMode(inID, kBluetoothIntelManufacturingExitResetOptionsNoReset);
        if (err)
            return err;
    }
    
    return kIOReturnSuccess;
}

IOReturn IntelBluetoothHostController::BluetoothHCIIntelReadBootParams(BluetoothHCIRequestID inID, BluetoothIntelBootParams * params)
{
    IOReturn err;
    IOBluetoothHCIRequest * request;
    
    err = PrepareRequestForNewCommand(inID, NULL, 0xFFFF);
    if ( err )
    {
        os_log(mInternalOSLogObject, "[IntelBluetoothHostController][BluetoothHCIIntelReadBootParams] Failed to prepare request for new command: 0x%x", err);
        return err;
    }
    
    err = SendHCIRequestFormatted(inID, 0xFC0D, sizeof(BluetoothIntelBootParams), params, "Hb", 0xFC0D, 0);
    if (err)
    {
        os_log(mInternalOSLogObject, "[IntelBluetoothHostController][BluetoothHCIIntelReadBootParams] ### ERROR: opCode = 0x%04X -- send request failed: 0x%x", 0xFC0D, err);
        return err;
    }
    
    if ( LookupRequest(inID, &request) || !request )
        return kIOReturnInvalid;
    
    if ( request->mStatus )
    {
        os_log(mInternalOSLogObject, "[IntelBluetoothHostController][BluetoothHCIIntelReadBootParams] Boot parameters are not obtained successfully -- status: %02x", request->mStatus);
        return request->mStatus;
    }

    os_log(mInternalOSLogObject, "[IntelBluetoothHostController][BluetoothHCIIntelReadBootParams] Device revision: %u -- Secure boot: %s -- OTP lock: %s -- API lock: %s -- Debug lock: %s -- Minimum firmware build: %u week: %u year: %u", params->deviceRevisionID, params->secureBoot ? "enabled" : "disabled", params->otpLock ? "enabled" : "disabled", params->apiLock ? "enabled" : "disabled", params->debugLock ? "enabled" : "disabled", params->minFirmwareBuildNumber, params->minFirmwareBuildWeek, 2000 + params->minFirmwareBuildYear);
    
    return kIOReturnSuccess;
}

IOReturn IntelBluetoothHostController::BluetoothHCIIntelReadVersionInfo(BluetoothHCIRequestID inID, UInt8 param, UInt8 * response)
{
    IOReturn err;
    IOBluetoothHCIRequest * request;
    
    if ( !response )
        return kIOReturnInvalid;
    
    err = PrepareRequestForNewCommand(inID, NULL, 0xFFFF);
    if ( err )
    {
        os_log(mInternalOSLogObject, "[IntelBluetoothHostController][BluetoothHCIIntelReadVersionInfo] Failed to prepare request for new command: 0x%x", err);
        return err;
    }
    
    if ( param == 0x00 )
        err = SendHCIRequestFormatted(inID, 0xFC05, sizeof(BluetoothIntelVersionInfo), response, "Hb", 0xFC05, 0);
    else if ( param == 0xFF )
        err = SendHCIRequestFormatted(inID, 0xFC05, kBluetoothHCICommandPacketMaxDataSize, response, "Hbb", 0xFC05, 1, 0xFF);
    else
    {
        os_log(mInternalOSLogObject, "[IntelBluetoothHostController][BluetoothHCIIntelReadVersionInfo] Invalid parameter (0x%02X), should be either 0x00 or 0xFF. ", param);
        return kIOReturnInvalid;
    }
    
    if ( err )
    {
        os_log(mInternalOSLogObject, "[IntelBluetoothHostController][BluetoothHCIIntelReadVersionInfo] ### ERROR: opCode = 0x%04X -- send request failed: 0x%x", 0xFC05, err);
        return err;
    }
    
    if ( LookupRequest(inID, &request) || !request )
        return kIOReturnInvalid;
    
    if ( request->mStatus )
    {
        os_log(mInternalOSLogObject, "[IntelBluetoothHostController][BluetoothHCIIntelReadVersionInfo] Intel Read Version command failed: 0x%x", request->mStatus);
        return kIOReturnIOError;
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
        os_log(mInternalOSLogObject, "[IntelBluetoothHostController][BluetoothHCIIntelReadDebugFeatures] Failed to prepare request for new command: 0x%x", err);
        return err;
    }
    
    /* Intel controller supports two pages, each page is of 128-bit
     * feature bit mask. And each bit defines specific feature support
     */
    response = IONewZero(UInt8, sizeof(features->page1) + 2);
    err = SendHCIRequestFormatted(inID, 0xFCA6, sizeof(features->page1) + 2, response, "Hbb", 0xFCA6, 1, 1);
    if (err)
    {
        os_log(mInternalOSLogObject, "[IntelBluetoothHostController][BluetoothHCIIntelReadDebugFeatures] ### ERROR: opCode = 0x%04X -- send request failed -- failed to read supported features for page 1: 0x%x", 0xFCA6, err);
        return err;
    }
    
    memcpy(features->page1, response + 2, sizeof(features->page1));
    IOSafeDeleteNULL(response, UInt8, sizeof(features->page1) + 2);

    return kIOReturnSuccess;
}

IOReturn IntelBluetoothHostController::BluetoothHCIIntelSetDebugFeatures(BluetoothHCIRequestID inID, const BluetoothIntelDebugFeatures * features)
{
    IOReturn err;
    
    if (!features)
        return kIOReturnInvalid;

    if (!(features->page1[0] & 0x3F))
    {
        os_log(mInternalOSLogObject, "[IntelBluetoothHostController][BluetoothHCIIntelSetDebugFeatures] Telemetry exception format not supported.");
        return kIOReturnSuccess;
    }

    err = PrepareRequestForNewCommand(inID, NULL, 0xFFFF);
    if ( err )
    {
        os_log(mInternalOSLogObject, "[IntelBluetoothHostController][BluetoothHCIIntelSetDebugFeatures] Failed to prepare request for new command: 0x%x", err);
        return err;
    }
    
    err = SendHCIRequestFormatted(inID, 0xFC8B, 0, NULL, "Hbbbbbbbbbbbb", 0xFC8B, 11, 0x0A, 0x92, 0x02, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
    if (err)
    {
        os_log(mInternalOSLogObject, "[IntelBluetoothHostController][BluetoothHCIIntelSetDebugFeatures] ### ERROR: opCode = 0x%04X -- send request failed -- failed to set telemetry DDC event mask: 0x%x", 0xFC8B, err);
        return err;
    }

    return kIOReturnSuccess;
}

IOReturn IntelBluetoothHostController::BluetoothHCIIntelTurnOffDeviceLED(BluetoothHCIRequestID inID)
{
    IOReturn err;
    
    err = PrepareRequestForNewCommand(inID, NULL, 0xFFFF);
    if ( err )
    {
        os_log(mInternalOSLogObject, "[IntelBluetoothHostController][BluetoothHCIIntelTurnOffDeviceLED] Failed to prepare request for new command: 0x%x", err);
        return err;
    }
    
    err = SendHCIRequestFormatted(inID, 0xFC3F, 0, NULL, "Hb", 0xFC3F, 0);
    if (err)
    {
        os_log(mInternalOSLogObject, "[IntelBluetoothHostController][BluetoothHCIIntelTurnOffDeviceLED] ### ERROR: opCode = 0x%04X -- send request failed -- failed to turn off device LED: 0x%x", 0xFC3F, err);
        return err;
    }
    
    return kIOReturnSuccess;
}

IOReturn IntelBluetoothHostController::BluetoothHCIIntelWriteDDC(BluetoothHCIRequestID inID, UInt8 * data, UInt8 dataSize)
{
    IOReturn err;
    BluetoothHCICommandPacket cmd =
    {
        .opCode = 0xFC8B,
        .dataSize = dataSize
    };
    memcpy(cmd.data, data, dataSize);
    
    err = SendRawHCICommand(inID, (char *) &cmd, cmd.dataSize + kBluetoothHCICommandPacketHeaderSize, NULL, 0);
    if (err)
    {
        os_log(mInternalOSLogObject, "[IntelBluetoothHostController][BluetoothHCIIntelWriteDDC] ### ERROR: opCode = 0x%04X -- send request failed: 0x%x", 0xFC8B, err);
        return err;
    }
    
    return kIOReturnSuccess;
}

IOReturn IntelBluetoothHostController::DownloadFirmwarePayload(BluetoothHCIRequestID inID, OSData * fwData, size_t offset)
{
    IOReturn err;
    BluetoothHCICommandPacket cmd;
    UInt8 * fwPtr;
    UInt32 fragmentSize;

    fwPtr = (UInt8 *) fwData->getBytesNoCopy() + offset;
    fragmentSize = 0;
    err = kIOReturnInvalid;

    while (fwPtr - (UInt8 *) fwData->getBytesNoCopy() < fwData->getLength())
    {
        cmd.opCode   = *(BluetoothHCICommandOpCode *)(fwPtr + fragmentSize);
        cmd.dataSize = *(UInt8 *)(fwPtr + fragmentSize + sizeof(BluetoothHCICommandOpCode));

        fragmentSize += kBluetoothHCICommandPacketHeaderSize + cmd.dataSize;

        /* The parameter length of the secure send command requires
         * a 4 byte alignment. It happens so that the firmware file
         * contains proper Intel_NOP commands to align the fragments
         * as needed.
         *
         * Send set of commands with 4 byte alignment from the
         * firmware data buffer as a single Data fragement.
         */
        if (!(fragmentSize % 4))
        {
            err = BluetoothHCIIntelSecureSend(inID, 0x01, fragmentSize, fwPtr);
            if (err)
            {
                os_log(mInternalOSLogObject, "[IntelBluetoothHostController][DownloadFirmwarePayload] BluetoothHCIIntelSecureSend() failed -- cannot send firmware data: 0x%x", err);
                return err;
            }

            fwPtr += fragmentSize;
            fragmentSize = 0;
        }
    }

    return err;
}

IOReturn IntelBluetoothHostController::SecureSendSFIRSAFirmwareHeader(BluetoothHCIRequestID inID, OSData * fwData)
{
    IOReturn err;

    /* Start the firmware download transaction with the Init fragment
     * represented by the 128 bytes of CSS header.
     */
    
    err = BluetoothHCIIntelSecureSend(inID, 0x00, 128, (UInt8 *) fwData->getBytesNoCopy());
    if (err)
    {
        os_log(mInternalOSLogObject, "[IntelBluetoothHostController][SecureSendSFIRSAFirmwareHeader] Failed to send firmware header: %d", err);
        return err;
    }

    /* Send the 256 bytes of public key information from the firmware
     * as the PKey fragment.
     */
    err = BluetoothHCIIntelSecureSend(inID, 0x03, 256, (UInt8 *) fwData->getBytesNoCopy() + 128);
    if (err)
    {
        os_log(mInternalOSLogObject, "[IntelBluetoothHostController][SecureSendSFIRSAFirmwareHeader] Failed to send firmware PKey: %d", err);
        return err;
    }

    /* Send the 256 bytes of signature information from the firmware
     * as the Sign fragment.
     */
    err = BluetoothHCIIntelSecureSend(inID, 0x02, 256, (UInt8 *) fwData->getBytesNoCopy() + 388);
    if (err)
    {
        os_log(mInternalOSLogObject, "[IntelBluetoothHostController][SecureSendSFIRSAFirmwareHeader] Failed to send firmware signature: %d", err);
        return err;
    }

    return kIOReturnSuccess;
}

IOReturn IntelBluetoothHostController::SecureSendSFIECDSAFirmwareHeader(BluetoothHCIRequestID inID, OSData * fwData)
{
    IOReturn err;

    /* Start the firmware download transaction with the Init fragment
     * represented by the 128 bytes of CSS header.
     */
    err = BluetoothHCIIntelSecureSend(inID, 0x00, 128, (UInt8 *) fwData->getBytesNoCopy() + 644);
    if (err)
    {
        os_log(mInternalOSLogObject, "[IntelBluetoothHostController][SecureSendSFIECDSAFirmwareHeader] Failed to send firmware header: %d", err);
        return err;
    }

    /* Send the 96 bytes of public key information from the firmware
     * as the PKey fragment.
     */
    err = BluetoothHCIIntelSecureSend(inID, 0x03, 96, (UInt8 *) fwData->getBytesNoCopy() + 644 + 128);
    if (err)
    {
        os_log(mInternalOSLogObject, "[IntelBluetoothHostController][SecureSendSFIECDSAFirmwareHeader] Failed to send firmware PKey: %d", err);
        return err;
    }

    /* Send the 96 bytes of signature information from the firmware
     * as the Sign fragment
     */
    err = BluetoothHCIIntelSecureSend(inID, 0x02, 96, (UInt8 *) fwData->getBytesNoCopy() + 644 + 224);
    if (err)
    {
        os_log(mInternalOSLogObject, "[IntelBluetoothHostController][SecureSendSFIECDSAFirmwareHeader] Failed to send firmware signature: %d", err);
        return err;
    }
    return kIOReturnSuccess;
}

bool IntelBluetoothHostController::ParseFirmwareVersion(UInt8 number, UInt8 week, UInt8 year, OSData * fwData, UInt32 * bootAddress)
{
    UInt8 * fwPtr;
    BluetoothHCICommandPacket cmd;
    BluetoothIntelCommandWriteBootParams * params;

    fwPtr = (UInt8 *) fwData->getBytesNoCopy();
    while (fwPtr - (UInt8 *) fwData->getBytesNoCopy() < fwData->getLength())
    {
        cmd.opCode   = *(BluetoothHCICommandOpCode *) fwPtr;
        cmd.dataSize = *(UInt8 *) (fwPtr + sizeof(BluetoothHCICommandOpCode));

        /* Each SKU has a different reset parameter to use in the
         * HCI_Intel_Reset command and it is embedded in the firmware
         * data. So, instead of using static value per SKU, check
         * the firmware data and save it for later use.
         */
        if (cmd.opCode == BluetoothHCIMakeCommandOpCode(kBluetoothHCICommandGroupVendorSpecific, kBluetoothHCIIntelCommandWriteBootParams))
        {
            params = (BluetoothIntelCommandWriteBootParams *) (fwPtr + kBluetoothHCICommandPacketHeaderSize);

            os_log(mInternalOSLogObject, "[IntelBluetoothHostController][ParseFirmwareVersion] Boot Address: 0x%x -- Firmware Version: %u-%u.%u", params->bootAddress, params->firmwareBuildNumber, params->firmwareBuildWeek, params->firmwareBuildYear);

            return (number == params->firmwareBuildNumber && week == params->firmwareBuildWeek && year == params->firmwareBuildYear);
        }
        
        fwPtr += kBluetoothHCICommandPacketHeaderSize + cmd.dataSize;
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
