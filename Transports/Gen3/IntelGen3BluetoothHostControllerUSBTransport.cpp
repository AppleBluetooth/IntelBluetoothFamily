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

    mFirmwareCandidates = fwCandidates;
    mNumFirmwares = fwCount;
    setProperty("ActiveBluetoothControllerVendor", "Intel - New Bootloader");
    return true;
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

    if ( !version || !data )
        return kIOReturnInvalid;

    /* Event parameters contatin multiple TLVs. Read each of them
     * and only keep the required data. Also, it use existing legacy
     * version field like hw_platform, hw_variant, and fw_variant
     * to keep the existing setup flow
     */
    while ( data && dataSize )
    {
        tlv = (BluetoothIntelTLV *) data;

        /* Make sure there is enough data */
        if ( dataSize < tlv->length + sizeof(BluetoothIntelTLV) )
            break;

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
                /* If image type is Operational firmware, then
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
                /* If image type is Operational firmware, then
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

IOReturn IntelGen3BluetoothHostControllerUSBTransport::DownloadFirmwareWL(void * ver, BluetoothIntelBootParams * params, UInt32 * bootAddress)
{
    IntelBluetoothHostController * controller = OSDynamicCast(IntelBluetoothHostController, mBluetoothController);
    if ( !controller )
        return kIOReturnInvalid;

    IOReturn err;
    UInt32 callTime;
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
    if ( version->imageType == kBluetoothHCIIntelImageTypeFirmware )
    {
        os_log(mInternalOSLogObject, "**** [IntelGen2BluetoothHostControllerUSBTransport][DownloadFirmware] -- Operational firmware is present! Calling CheckDeviceAddress()... ****\n");
        controller->mBootloaderMode = false;
        controller->CheckDeviceAddress();
    }

    /* If the OTP has no valid Bluetooth device address, then there will
     * also be no valid address for the operational firmware.
     */
    if ( version->otpDeviceAddress.data[0] == 0 && version->otpDeviceAddress.data[1] == 0 && version->otpDeviceAddress.data[2] == 0 && version->otpDeviceAddress.data[3] == 0 && version->otpDeviceAddress.data[4] == 0 && version->otpDeviceAddress.data[5] == 0 )
    {
        os_log(mInternalOSLogObject, "**** [IntelGen3BluetoothHostControllerUSBTransport][DownloadFirmwareWL] -- No device address configured! ****\n");
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
        os_log(mInternalOSLogObject, "**** [IntelGen3BluetoothHostControllerUSBTransport][DownloadFirmwareWL] -- Invalid size of firmware file: %u ****\n", fwData->getLength());
        return kIOReturnUnsupported;
    }
    
    callTime = mBluetoothFamily->GetCurrentTime();

    controller->mDownloading = true;
    
    /* Skip download if firmware has the same version */
    if ( controller->CheckFirmwareVersion(version->firmwareBuildNumber, version->firmwareBuildWeek, version->firmwareBuildYear, fwData, bootAddress) )
    {
        os_log(mInternalOSLogObject, "**** [IntelGen3BluetoothHostControllerUSBTransport][DownloadFirmware] -- Firmware already loaded! ****\n");
        controller->mDownloading = false;
        controller->mFirmwareLoaded = true;
        setProperty("FirmwareLoaded", true);
        return kIOReturnSuccess;
    }

    
    /* The firmware variant determines if the device is in bootloader
     * mode or is running operational firmware. The value 0x01 identifies
     * the bootloader and the value 0x03 identifies the operational
     * firmware.
     *
     * If the firmware version has changed that means it needs to be reset
     * to bootloader when operational so the new firmware can be loaded.
     */
    if ( version->imageType == kBluetoothHCIIntelImageTypeFirmware )
    {
        err = controller->ResetToBootloader(true);
        if ( err )
            return err;
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
        os_log(mInternalOSLogObject, "**** [IntelGen3BluetoothHostControllerUSBTransport][DownloadFirmware] -- Invalid CSS Header version! ****\n");
        err = kIOReturnInvalid;
        goto done;
    }
    
    if ( IntelCNVXExtractHardwareVariant(version->cnviBT) <= 0x14 )
    {
        if ( version->sbeType != 0x00 )
        {
            os_log(mInternalOSLogObject, "**** [IntelGen3BluetoothHostControllerUSBTransport][DownloadFirmware] -- Invalid SBE type for hardware variant (%d)! ****\n", IntelCNVXExtractHardwareVariant(version->cnviBT));
            err = kIOReturnInvalid;
            goto done;
        }

        err = controller->SecureSendSFIRSAFirmwareHeader(fwData);
        if ( err )
            goto done;

        err = controller->DownloadFirmwarePayload(fwData, kIntelRSAHeaderLength);
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
            os_log(mInternalOSLogObject, "**** [IntelGen3BluetoothHostControllerUSBTransport][DownloadFirmware] -- Invalid CSS Header version! ****\n");
            err = kIOReturnInvalid;
            goto done;
        }

        if ( version->sbeType == 0x00 )
        {
            err = controller->SecureSendSFIRSAFirmwareHeader(fwData);
            if ( err )
                goto done;
        }
        else
        {
            err = controller->SecureSendSFIECDSAFirmwareHeader(fwData);
            if ( err )
                goto done;
        }
        
        err = controller->DownloadFirmwarePayload(fwData, kIntelRSAHeaderLength + kIntelECDSAHeaderLength);
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
    {
done:
        controller->ResetToBootloader(false);
        return err;
    }
    
    return kIOReturnSuccess;
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
