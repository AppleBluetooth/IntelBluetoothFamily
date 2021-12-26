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

#include "IntelGen2BluetoothHostControllerUSBTransport.h"

#define super IntelBluetoothHostControllerUSBTransport
OSDefineMetaClassAndStructors(IntelGen2BluetoothHostControllerUSBTransport, super)

bool IntelGen2BluetoothHostControllerUSBTransport::start(IOService * provider)
{
    if ( !super::start(provider) )
        return false;

    mFirmwareCandidates = fwCandidates;
    mNumFirmwares = fwCount;
    setProperty("ActiveBluetoothControllerVendor", "Intel - Legacy Bootloader");
    return true;
}

IOReturn IntelGen2BluetoothHostControllerUSBTransport::GetFirmwareNameWL(void * ver, BluetoothIntelBootParams * params, const char * suffix, char * fwName)
{
    BluetoothIntelVersionInfo * version = (BluetoothIntelVersionInfo *) ver;
    char firmwareName[64];
    
    switch ( version->hardwareVariant )
    {
        case kBluetoothIntelHardwareVariantSfP:
        case kBluetoothIntelHardwareVariantWsP:
            snprintf(firmwareName, sizeof(firmwareName), "ibt-%u-%u.%s", (UInt16) version->hardwareVariant, (UInt16) params->deviceRevisionID, suffix);
            break;
        case kBluetoothIntelHardwareVariantJfP:
        case kBluetoothIntelHardwareVariantThP:
        case kBluetoothIntelHardwareVariantHrP:
        case kBluetoothIntelHardwareVariantCcP:
            snprintf(firmwareName, sizeof(firmwareName), "ibt-%u-%u-%u.%s", (UInt16) version->hardwareVariant, (UInt16) version->hardwareRevision, (UInt16) version->firmwareRevision, suffix);
            break;
        default:
            return kIOReturnInvalid;
    }

    strcpy(fwName, firmwareName, 64);
    return kIOReturnSuccess;
}

IOReturn IntelGen2BluetoothHostControllerUSBTransport::DownloadFirmwareWL(void * ver, BluetoothIntelBootParams * params, UInt32 * bootAddress)
{
    IntelBluetoothHostController * controller = OSDynamicCast(IntelBluetoothHostController, mBluetoothController);
    if ( !controller )
        return false;

    IOReturn err, ret;
    UInt32 callTime;
    BluetoothIntelVersionInfo * version = (BluetoothIntelVersionInfo *) ver;
    OSData * fwData;
    BluetoothHCIRequestID id;

    if ( !version || !params )
        return kIOReturnInvalid;

    /* The firmware variant determines if the device is in bootloader
     * mode or is running operational firmware. The value 0x06 identifies
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
    if ( version->firmwareVariant == kBluetoothHCIIntelFirmwareVariantFirmware )
    {
        controller->mBootloaderMode = false;
        controller->CheckDeviceAddress();

        /* SfP and WsP don't seem to update the firmware version on file
         * so version checking is currently possible.
         */
        if ( version->hardwareVariant == kBluetoothIntelHardwareVariantSfP || version->hardwareVariant == kBluetoothIntelHardwareVariantWsP )
            return kIOReturnSuccess;

        /* Proceed to download to check if the version matches */
        goto download;
    }

    /* Read the secure boot parameters to identify the operating
     * details of the bootloader.
     */
    err = controller->HCIRequestCreate(&id);
    if ( err )
    {
        REQUIRE_NO_ERR(err);
        return err;
    }
    ret = controller->BluetoothHCIIntelReadBootParams(id, params);
    controller->HCIRequestDelete(NULL, id);
    if ( ret )
        return ret;

    /* It is required that every single firmware fragment is acknowledged
     * with a command complete event. If the boot parameters indicate
     * that this bootloader does not send them, then abort the setup.
     */
    if ( params->limitedCCE != 0x00 )
    {
        os_log(mInternalOSLogObject, "**** [IntelGen2BluetoothHostControllerUSBTransport][DownloadFirmware] -- Unsupported firmware loading method: %u ****\n", params->limitedCCE);
        return kIOReturnInvalid;
    }

    /* If the OTP has no valid Bluetooth device address, then there will
     * also be no valid address for the operational firmware.
     */
    if ( params->otpDeviceAddress.data[0] == 0 && params->otpDeviceAddress.data[1] == 0 && params->otpDeviceAddress.data[2] == 0 && params->otpDeviceAddress.data[3] == 0 && params->otpDeviceAddress.data[4] == 0 && params->otpDeviceAddress.data[5] == 0 )
    {
        os_log(mInternalOSLogObject, "**** [IntelGen2BluetoothHostControllerUSBTransport][DownloadFirmware] -- No device address configured! ****\n");
        controller->mInvalidDeviceAddress = true;
    }
    
download:
    /* With this Intel bootloader only the hardware variant and device
     * revision information are used to select the right firmware for SfP
     * and WsP.
     *
     * The firmware filename is ibt-<hw_variant>-<dev_revid>.sfi.
     *
     * Currently the supported hardware variants are:
     *   11 (0x0b) for iBT3.0 (LnP/SfP)
     *   12 (0x0c) for iBT3.5 (WsP)
     *
     * For ThP/JfP and for future SKU's, the FW name varies based on HW
     * variant, HW revision and FW revision, as these are dependent on CNVi
     * and RF Combination.
     *
     *   17 (0x11) for iBT3.5 (JfP)
     *   18 (0x12) for iBT3.5 (ThP)
     *
     * The firmware file name for these will be
     * ibt-<hw_variant>-<hw_revision>-<fw_revision>.sfi.
     *
     */
    
    ret = GetFirmware(version, params, "sfi", &fwData);
    if ( ret )
    {
        if ( !controller->mBootloaderMode )
        {
            /* Firmware has already been loaded */
            controller->mFirmwareLoaded = true;
            setProperty("FirmwareLoaded", true);
            return kIOReturnSuccess;
        }
        return ret;
    }

    if ( fwData->getLength() < 644 )
    {
        os_log(mInternalOSLogObject, "**** [IntelGen2BluetoothHostControllerUSBTransport][DownloadFirmware] -- Size of firmware file is invalid: %u! ****\n", fwData->getLength());
        return kIOReturnUnsupported;
    }

    callTime = mBluetoothFamily->GetCurrentTime();

    controller->mDownloading = true;

    /* Start firmware downloading and get boot parameter */
    
    /* SfP and WsP don't seem to update the firmware version on file
     * so version checking is currently not possible.
     */
    switch ( version->hardwareVariant )
    {
        case kBluetoothIntelHardwareVariantSfP:
        case kBluetoothIntelHardwareVariantWsP:
            /* Skip version checking */
            break;
        default:
            /* Skip download if firmware has the same version */
            if ( controller->ParseFirmwareVersion(version->firmwareBuildNum, version->firmwareBuildWeek, version->firmwareBuildYear, fwData, bootAddress) )
            {
                os_log(mInternalOSLogObject, "**** [IntelGen2BluetoothHostControllerUSBTransport][DownloadFirmware] -- Firmware already loaded! ****\n");
                controller->mFirmwareLoaded = true;
                setProperty("FirmwareLoaded", true);
                return kIOReturnSuccess;
            }
    }

    /* The firmware variant determines if the device is in bootloader
     * mode or is running operational firmware. The value 0x06 identifies
     * the bootloader and the value 0x23 identifies the operational
     * firmware.
     *
     * If the firmware version has changed that means it needs to be reset
     * to bootloader when operational so the new firmware can be loaded.
     */
    if ( version->firmwareVariant == kBluetoothHCIIntelFirmwareVariantFirmware )
    {
        ret = kIOReturnInvalid;
        goto done;
    }

    ret = controller->SecureSendSFIRSAFirmwareHeader(fwData);
    if ( ret )
        goto done;

    ret = controller->DownloadFirmwarePayload(fwData, kIntelRSAHeaderLength);
    if ( ret )
        goto done;

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
    ret = controller->WaitForFirmwareDownload(callTime, 5000);
    if ( ret == kIOReturnTimeout )
    {
done:
        controller->ResetToBootloader();
    }
    return ret;
}

OSMetaClassDefineReservedUnused(IntelGen2BluetoothHostControllerUSBTransport, 0)
OSMetaClassDefineReservedUnused(IntelGen2BluetoothHostControllerUSBTransport, 1)
OSMetaClassDefineReservedUnused(IntelGen2BluetoothHostControllerUSBTransport, 2)
OSMetaClassDefineReservedUnused(IntelGen2BluetoothHostControllerUSBTransport, 3)
OSMetaClassDefineReservedUnused(IntelGen2BluetoothHostControllerUSBTransport, 4)
OSMetaClassDefineReservedUnused(IntelGen2BluetoothHostControllerUSBTransport, 5)
OSMetaClassDefineReservedUnused(IntelGen2BluetoothHostControllerUSBTransport, 6)
OSMetaClassDefineReservedUnused(IntelGen2BluetoothHostControllerUSBTransport, 7)
OSMetaClassDefineReservedUnused(IntelGen2BluetoothHostControllerUSBTransport, 8)
OSMetaClassDefineReservedUnused(IntelGen2BluetoothHostControllerUSBTransport, 9)
OSMetaClassDefineReservedUnused(IntelGen2BluetoothHostControllerUSBTransport, 10)
OSMetaClassDefineReservedUnused(IntelGen2BluetoothHostControllerUSBTransport, 11)
OSMetaClassDefineReservedUnused(IntelGen2BluetoothHostControllerUSBTransport, 12)
OSMetaClassDefineReservedUnused(IntelGen2BluetoothHostControllerUSBTransport, 13)
OSMetaClassDefineReservedUnused(IntelGen2BluetoothHostControllerUSBTransport, 14)
OSMetaClassDefineReservedUnused(IntelGen2BluetoothHostControllerUSBTransport, 15)
OSMetaClassDefineReservedUnused(IntelGen2BluetoothHostControllerUSBTransport, 16)
OSMetaClassDefineReservedUnused(IntelGen2BluetoothHostControllerUSBTransport, 17)
OSMetaClassDefineReservedUnused(IntelGen2BluetoothHostControllerUSBTransport, 18)
OSMetaClassDefineReservedUnused(IntelGen2BluetoothHostControllerUSBTransport, 19)
OSMetaClassDefineReservedUnused(IntelGen2BluetoothHostControllerUSBTransport, 20)
OSMetaClassDefineReservedUnused(IntelGen2BluetoothHostControllerUSBTransport, 21)
OSMetaClassDefineReservedUnused(IntelGen2BluetoothHostControllerUSBTransport, 22)
OSMetaClassDefineReservedUnused(IntelGen2BluetoothHostControllerUSBTransport, 23)
