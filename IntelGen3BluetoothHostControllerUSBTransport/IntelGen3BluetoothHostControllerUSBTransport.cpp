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

#include "IntelGen3BluetoothHostControllerUSBTransport.h"

#define super IntelBluetoothHostControllerUSBTransport
OSDefineMetaClassAndStructors(IntelGen3BluetoothHostControllerUSBTransport, super)

bool IntelGen3BluetoothHostControllerUSBTransport::start(IOService * provider)
{
    if ( !super::start(provider) )
        return false;

    IntelBluetoothHostController * controller = OSDynamicCast(IntelBluetoothHostController, mBluetoothController);
    if ( !controller )
        return false;

    IOReturn err;
    BluetoothHCIRequestID id;
    BluetoothIntelVersionInfoTLV version;
    BluetoothIntelDebugFeatures features;
    OSData * fwData;
    UInt32 bootAddress;

    err = ParseVersionInfoTLV(&version, (UInt8 *) controller->mVersionInfo, kBluetoothHCICommandPacketMaxDataSize);
    if ( err )
    {
        os_log(mInternalOSLogObject, "[IntelGen3BluetoothHostControllerUSBTransport][start] Failed to parse TLV version information!");
        return false;
    }
    
    if ( IntelCNVXExtractHardwarePlatform(version.cnviBT) != 0x37 )
    {
        os_log(mInternalOSLogObject, "[IntelGen3BluetoothHostControllerUSBTransport][start] Unsupported hardware platform: 0x%2x", IntelCNVXExtractHardwarePlatform(version.cnviBT));
        return false;
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
            os_log(mInternalOSLogObject, "[IntelGen3BluetoothHostControllerUSBTransport][start] This controller is not an Intel new bootloader device!!!");
            return false;
        case 0x18:
            /* Valid LE States quirk for GfP */
            controller->mValidLEStates = true;
        case 0x17:
        case 0x19:
            setProperty("ActiveBluetoothControllerVendor", "Intel - New Bootloader");

            /* Display version information of TLV type */
            controller->PrintVersionInfo(&version);

            /* Apply the device specific HCI quirks for TLV based devices
             *
             * All TLV based devices support WBS
             */
            controller->mWidebandSpeechSupported = true;

            /* Setup MSFT Extension support */
            controller->SetMicrosoftExtensionOpCode(IntelCNVXExtractHardwareVariant(version.cnviBT));

            /* Set the default boot parameter to 0x0 and it is updated to
             * SKU specific boot parameter after reading Intel_Write_Boot_Params
             * command while downloading the firmware.
             */
            bootAddress = 0x00000000;

            controller->mBootloaderMode = true;

            controller->HCIRequestCreate(&id);
            err = DownloadFirmware(id, &version, NULL, &bootAddress);
            controller->HCIRequestDelete(NULL, id);
            if ( err )
                return false;

            /* check if controller is already having an operational firmware */
            if ( version.imageType == 0x03 )
                goto finish;

            err = controller->BootDevice(bootAddress);
            if ( err )
                return false;

            controller->mBootloaderMode = false;

            err = GetFirmware(&version, NULL, "ddc", &fwData);
            if ( !err )
            {
                /* Once the device is running in operational mode, it needs to
                 * apply the device configuration (DDC) parameters.
                 *
                 * The device can work without DDC parameters, so even if it
                 * fails to load the file, no need to fail the setup.
                 */
                controller->HCIRequestCreate(&id);
                controller->LoadDDCConfig(id, fwData);
                controller->HCIRequestDelete(NULL, id);
            }

            /* Read the Intel supported features and if new exception formats
             * supported, need to load the additional DDC config to enable.
             */
            controller->HCIRequestCreate(&id);
            err = controller->BluetoothHCIIntelReadDebugFeatures(id, &features);
            controller->HCIRequestDelete(NULL, id);
            if ( !err )
            {
                /* Set DDC mask for available debug features */
                controller->HCIRequestCreate(&id);
                controller->BluetoothHCIIntelSetDebugFeatures(id, &features);
                controller->HCIRequestDelete(NULL, id);
            }

            /* Read the Intel version information after loading the FW  */
            controller->HCIRequestCreate(&id);
            err = controller->BluetoothHCIIntelReadVersionInfo(id, 0xFF, (UInt8 *) &version);
            controller->HCIRequestDelete(NULL, id);
            if ( err )
                return false;

            controller->PrintVersionInfo(&version);

finish:
            /* Set the event mask for Intel specific vendor events. This enables
             * a few extra events that are useful during general operation. It
             * does not enable any debugging related events.
             *
             * The device will function correctly without these events enabled
             * and thus no need to fail the setup.
             */
            controller->HCIRequestCreate(&id);
            controller->BluetoothHCIIntelSetEventMask(id, false);
            controller->HCIRequestDelete(NULL, id);

            return true;
        default:
            os_log(mInternalOSLogObject, "[IntelGen3BluetoothHostControllerUSBTransport][start] Unsupported hardware variant: %u", IntelCNVXExtractHardwareVariant(version.cnviBT));
            return false;
    }
}

IOReturn IntelGen3BluetoothHostControllerUSBTransport::GetFirmwareNameWL(void * ver, BluetoothIntelBootParams * params, const char * suffix, char * fwName)
{
    BluetoothIntelVersionInfoTLV * version = (BluetoothIntelVersionInfoTLV *) ver;
    char firmwareName[64];
    /* The firmware file name for new generation controllers will be
     * ibt-<cnvi_top type+cnvi_top step>-<cnvr_top type+cnvr_top step>
     */
    snprintf(firmwareName, 64, "ibt-%04x-%04x.%s", IntelMakeCNVXTopEndianSwap(IntelCNVXTopExtractType(version->cnviTop), IntelCNVXTopExtractStep(version->cnviTop)), IntelMakeCNVXTopEndianSwap(IntelCNVXTopExtractType(version->cnvrTop), IntelCNVXTopExtractStep(version->cnvrTop)), suffix);
    strcpy(fwName, firmwareName, 64);
    return kIOReturnSuccess;
}

IOReturn IntelGen3BluetoothHostControllerUSBTransport::ParseVersionInfoTLV(BluetoothIntelVersionInfoTLV * version, UInt8 * data, IOByteCount dataSize)
{
    BluetoothIntelTLV * tlv;
    
    /* Event parameters contatin multiple TLVs. Read each of them
     * and only keep the required data. Also, it use existing legacy
     * version field like hw_platform, hw_variant, and fw_variant
     * to keep the existing setup flow
     */
    while ( dataSize )
    {
        tlv = (BluetoothIntelTLV *) data;

        /* Make sure there is enough data */
        if ( dataSize < tlv->length + sizeof(BluetoothIntelTLV) )
            return kIOReturnInvalid;

        switch ( tlv->type )
        {
            case kBluetoothIntelTLVTypeCNVITop:
                version->cnviTop = *(UInt32 *)(tlv->value);
                break;
            case kBluetoothIntelTLVTypeCNVRTop:
                version->cnvrTop = *(UInt32 *)(tlv->value);
                break;
            case kBluetoothIntelTLVTypeCNVIBT:
                version->cnviBT = *(UInt32 *)(tlv->value);
                break;
            case kBluetoothIntelTLVTypeCNVRBT:
                version->cnvrBT = *(UInt32 *)(tlv->value);
                break;
            case kBluetoothIntelTLVTypeDeviceRevisionID:
                version->deviceRevisionID = *(UInt16 *)(tlv->value);
                break;
            case kBluetoothIntelTLVTypeImageType:
                version->imageType = tlv->value[0];
                break;
            case kBluetoothIntelTLVTypeTimestamp:
                /* If image type is Operational firmware (0x03), then
                 * running FW Calendar Week and Year information can
                 * be extracted from Timestamp information
                 */
                version->firmwareBuildWeek = tlv->value[0];
                version->firmwareBuildYear = tlv->value[1];
                version->timestamp = *(UInt16 *)(tlv->value);
                break;
            case kBluetoothIntelTLVTypeBuildType:
                version->buildType = tlv->value[0];
                break;
            case kBluetoothIntelTLVTypeBuildNum:
                /* If image type is Operational firmware (0x03), then
                 * running FW build number can be extracted from the
                 * Build information
                 */
                version->firmwareBuildNumber = tlv->value[0];
                version->buildNumber = *(UInt32 *)(tlv->value);
                break;
            case kBluetoothIntelTLVTypeSecureBoot:
                version->secureBoot = tlv->value[0];
                break;
            case kBluetoothIntelTLVTypeOTPLock:
                version->otpLock = tlv->value[0];
                break;
            case kBluetoothIntelTLVTypeAPILock:
                version->apiLock = tlv->value[0];
                break;
            case kBluetoothIntelTLVTypeDebugLock:
                version->debugLock = tlv->value[0];
                break;
            case kBluetoothIntelTLVTypeMinimumFirmware:
                version->firmwareBuildNumber = tlv->value[0];
                version->firmwareBuildWeek = tlv->value[1];
                version->firmwareBuildYear = tlv->value[2];
                break;
            case kBluetoothIntelTLVTypeLimitedCCE:
                version->limitedCCE = tlv->value[0];
                break;
            case kBluetoothIntelTLVTypeSBEType:
                version->sbeType = tlv->value[0];
                break;
            case kBluetoothIntelTLVTypeOTPDeviceAddress:
                memcpy(&version->otpDeviceAddress, tlv->value, sizeof(BluetoothDeviceAddress));
                break;
            default:
                /* Ignore rest of information */
                break;
        }
        
        /* consume the current tlv and move to next */
        dataSize -= (tlv->length + sizeof(BluetoothIntelTLV));
        data     += (tlv->length + sizeof(BluetoothIntelTLV));
    }

    return kIOReturnSuccess;
}

IOReturn IntelGen3BluetoothHostControllerUSBTransport::GetFirmwareWL(void * version, BluetoothIntelBootParams * params, const char * suffix, OSData ** fwData)
{
    char fwName[64];
    char ** fwNames = IONew(char *, 1);
    fwNames[0] = fwName;
    
    if ( GetFirmwareName(version, params, suffix, fwName, sizeof(fwName)) )
    {
        os_log(mInternalOSLogObject, "[IntelGen3BluetoothHostControllerUSBTransport][GetFirmwareWL] Unsupported firmware name!");
        return kIOReturnInvalid;
    }
    setProperty("FirmwareName", fwName);
    
    mFirmware = OpenFirmwareManager::withNames(fwNames, 1, fwCandidates, fwCount);
    IOSafeDeleteNULL(fwNames, char *, 1);
    if ( !mFirmware )
    {
        os_log(mInternalOSLogObject, "[IntelGen3BluetoothHostControllerUSBTransport][GetFirmwareWL] Failed to obtain firmware file %s!!!", fwName);
        return kIOReturnUnsupported;
    }
    *fwData = mFirmware->getFirmwareUncompressed(fwName);
    
    os_log(mInternalOSLogObject, "[IntelGen3BluetoothHostControllerUSBTransport][GetFirmwareWL] Found firmware file: %s", fwName);
    return kIOReturnSuccess;
}

IOReturn IntelGen3BluetoothHostControllerUSBTransport::DownloadFirmwareWL(BluetoothHCIRequestID inID, void * ver, BluetoothIntelBootParams * params, UInt32 * bootAddress)
{
    IntelBluetoothHostController * controller = OSDynamicCast(IntelBluetoothHostController, mBluetoothController);
    if ( !controller )
        return false;

    IOReturn err;
    AbsoluteTime callTime;
    BluetoothIntelVersionInfoTLV * version = (BluetoothIntelVersionInfoTLV *) ver;
    OSData * fwData;
    UInt32 cssHeaderVersion;
    
    if ( !version || !bootAddress )
        return kIOReturnInvalid;

    /* The firmware variant determines if the device is in bootloader
     * mode or is running operational firmware. The value 0x03 identifies
     * the bootloader and the value 0x23 identifies the operational
     * firmware.
     *
     * When the operational firmware is already present, then only
     * the check for valid Bluetooth device address is needed. This
     * determines if the device will be added as configured or
     * unconfigured controller.
     *
     * It is not possible to use the Secure Boot Parameters in this
     * case since that command is only available in bootloader mode.
     */
    if ( version->imageType == 0x03 )
    {
        controller->mBootloaderMode = false;
        controller->CheckDeviceAddress(inID);
    }

    /* If the OTP has no valid Bluetooth device address, then there will
     * also be no valid address for the operational firmware.
     */
    if ( version->otpDeviceAddress.data[0] == 0 && version->otpDeviceAddress.data[1] == 0 && version->otpDeviceAddress.data[2] == 0 && version->otpDeviceAddress.data[3] == 0 && version->otpDeviceAddress.data[4] == 0 && version->otpDeviceAddress.data[5] == 0 )
    {
        os_log(mInternalOSLogObject, "[IntelGen3BluetoothHostControllerUSBTransport][DownloadFirmwareWL] No device address configured!");
        controller->mInvalidDeviceAddress = true;
    }

    err = GetFirmware(version, NULL, "sfi", &fwData);
    if ( err )
    {
        if ( !controller->mBootloaderMode )
        {
            /* Firmware has already been loaded */
            controller->mFirmwareLoaded = true;
            setProperty("FirmwareLoaded", true);
            return kIOReturnSuccess;
        }
        return err;
    }
    
    if ( fwData->getLength() < 644 )
    {
        os_log(mInternalOSLogObject, "[IntelGen3BluetoothHostControllerUSBTransport][DownloadFirmwareWL] Invalid size of firmware file: %u", fwData->getLength());
        return kIOReturnUnsupported;
    }
    
    callTime = mBluetoothFamily->GetCurrentTime();

    controller->mDownloading = true;

    /* Skip reading firmware file version in bootloader mode */
    if ( version->imageType != 0x01 )
    {
        /* Skip download if firmware has the same version */
        if ( controller->ParseFirmwareVersion(version->firmwareBuildNumber, version->firmwareBuildWeek, version->firmwareBuildYear, fwData, bootAddress) )
        {
            os_log(mInternalOSLogObject, "[IntelGen3BluetoothHostControllerUSBTransport][DownloadFirmware] Firmware already loaded!");
            controller->mFirmwareLoaded = true;
            setProperty("FirmwareLoaded", true);
            return kIOReturnSuccess;
        }
    }
    
    /* The firmware variant determines if the device is in bootloader
     * mode or is running operational firmware. The value 0x01 identifies
     * the bootloader and the value 0x03 identifies the operational
     * firmware.
     *
     * If the firmware version has changed that means it needs to be reset
     * to bootloader when operational so the new firmware can be loaded.
     */
    if ( version->imageType == 0x03 )
    {
        err = kIOReturnInvalid;
        goto done;
    }
    
    /* iBT hardware variants 0x0b, 0x0c, 0x11, 0x12, 0x13, 0x14 support
     * only RSA secure boot engine. Hence, the corresponding sfi file will
     * have RSA header of 644 bytes followed by Command Buffer.
     *
     * iBT hardware variants 0x17, 0x18 onwards support both RSA and ECDSA
     * secure boot engine. As a result, the corresponding sfi file will
     * have RSA header of 644, ECDSA header of 320 bytes followed by
     * Command Buffer.
     *
     * CSS Header byte positions 0x08 to 0x0B represent the CSS Header
     * version: RSA(0x00010000) , ECDSA (0x00020000)
     */
    cssHeaderVersion = *(UInt32 *)((UInt8 *) fwData->getBytesNoCopy() + kIntelCSSHeaderOffset);
    if ( cssHeaderVersion != 0x00010000 )
    {
        os_log(mInternalOSLogObject, "[IntelGen3BluetoothHostControllerUSBTransport][DownloadFirmware] Invalid CSS Header version!");
        err = kIOReturnInvalid;
        goto done;
    }
    
    if ( IntelCNVXExtractHardwareVariant(version->cnviBT) <= 0x14 )
    {
        if ( version->sbeType != 0x00 )
        {
            os_log(mInternalOSLogObject, "[IntelGen3BluetoothHostControllerUSBTransport][DownloadFirmware] Invalid SBE type for hardware variant (%d)!", IntelCNVXExtractHardwareVariant(version->cnviBT));
            err = kIOReturnInvalid;
            goto done;
        }

        err = controller->SecureSendSFIRSAFirmwareHeader(inID, fwData);
        if ( err )
            goto done;

        err = controller->DownloadFirmwarePayload(inID, fwData, kIntelRSAHeaderLength);
        if ( err )
            goto done;
    }
    
    else if ( IntelCNVXExtractHardwareVariant(version->cnviBT) >= 0x17 )
    {
        /* Check if CSS header for ECDSA follows the RSA header */
        if ( ((UInt8 *) fwData->getBytesNoCopy())[kIntelECDSAOffset] != 0x06 )
        {
            err = kIOReturnInvalid;
            goto done;
        }

        /* Check if the CSS Header version is ECDSA(0x00020000) */
        cssHeaderVersion = *(UInt32 *)((UInt8 *) fwData->getBytesNoCopy() + kIntelECDSAOffset + kIntelCSSHeaderOffset);
        if ( cssHeaderVersion != 0x00020000 )
        {
            os_log(mInternalOSLogObject, "[IntelGen3BluetoothHostControllerUSBTransport][DownloadFirmware] Invalid CSS Header version!");
            err = kIOReturnInvalid;
            goto done;
        }

        if ( version->sbeType == 0x00 )
        {
            err = controller->SecureSendSFIRSAFirmwareHeader(inID, fwData);
            if ( err )
                goto done;
        }
        else
        {
            err = controller->SecureSendSFIECDSAFirmwareHeader(inID, fwData);
            if ( err )
                goto done;
        }
        
        err = controller->DownloadFirmwarePayload(inID, fwData, kIntelRSAHeaderLength + kIntelECDSAHeaderLength);
        if ( err )
            goto done;
    }

    /* Before switching the device into operational mode and with that
     * booting the loaded firmware, wait for the bootloader notification
     * that all fragments have been successfully received.
     *
     * When the event processing receives the notification, then the
     * INTEL_DOWNLOADING flag will be cleared.
     *
     * The firmware loading should not take longer than 5 seconds
     * and thus just timeout if that happens and fail the setup
     * of this device.
     */
    err = controller->WaitForFirmwareDownload(callTime, 5000);
    if ( err == kIOReturnTimeout )
done:
        controller->ResetToBootloader(inID);
    return err;
}

OSMetaClassDefineReservedUnused(IntelGen3BluetoothHostControllerUSBTransport, 0)
OSMetaClassDefineReservedUnused(IntelGen3BluetoothHostControllerUSBTransport, 1)
OSMetaClassDefineReservedUnused(IntelGen3BluetoothHostControllerUSBTransport, 2)
OSMetaClassDefineReservedUnused(IntelGen3BluetoothHostControllerUSBTransport, 3)
OSMetaClassDefineReservedUnused(IntelGen3BluetoothHostControllerUSBTransport, 4)
OSMetaClassDefineReservedUnused(IntelGen3BluetoothHostControllerUSBTransport, 5)
OSMetaClassDefineReservedUnused(IntelGen3BluetoothHostControllerUSBTransport, 6)
OSMetaClassDefineReservedUnused(IntelGen3BluetoothHostControllerUSBTransport, 7)
OSMetaClassDefineReservedUnused(IntelGen3BluetoothHostControllerUSBTransport, 8)
OSMetaClassDefineReservedUnused(IntelGen3BluetoothHostControllerUSBTransport, 9)
OSMetaClassDefineReservedUnused(IntelGen3BluetoothHostControllerUSBTransport, 10)
OSMetaClassDefineReservedUnused(IntelGen3BluetoothHostControllerUSBTransport, 11)
OSMetaClassDefineReservedUnused(IntelGen3BluetoothHostControllerUSBTransport, 12)
OSMetaClassDefineReservedUnused(IntelGen3BluetoothHostControllerUSBTransport, 13)
OSMetaClassDefineReservedUnused(IntelGen3BluetoothHostControllerUSBTransport, 14)
OSMetaClassDefineReservedUnused(IntelGen3BluetoothHostControllerUSBTransport, 15)
OSMetaClassDefineReservedUnused(IntelGen3BluetoothHostControllerUSBTransport, 16)
OSMetaClassDefineReservedUnused(IntelGen3BluetoothHostControllerUSBTransport, 17)
OSMetaClassDefineReservedUnused(IntelGen3BluetoothHostControllerUSBTransport, 18)
OSMetaClassDefineReservedUnused(IntelGen3BluetoothHostControllerUSBTransport, 19)
OSMetaClassDefineReservedUnused(IntelGen3BluetoothHostControllerUSBTransport, 20)
OSMetaClassDefineReservedUnused(IntelGen3BluetoothHostControllerUSBTransport, 21)
OSMetaClassDefineReservedUnused(IntelGen3BluetoothHostControllerUSBTransport, 22)
OSMetaClassDefineReservedUnused(IntelGen3BluetoothHostControllerUSBTransport, 23)
