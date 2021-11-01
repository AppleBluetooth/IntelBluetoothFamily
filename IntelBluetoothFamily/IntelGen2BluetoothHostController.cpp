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

#include "IntelGen2BluetoothHostController.h"
#include "../IntelBluetoothHostControllerUSBTransport/IntelBluetoothHostControllerUSBTransport.h"

#define super IntelBluetoothHostController
OSDefineMetaClassAndStructors(IntelGen2BluetoothHostController, super)

bool IntelGen2BluetoothHostController::start(IOService * provider)
{
    if (!super::start(provider))
        return false;
    
    IOReturn err;
    BluetoothHCIRequestID id;
    BluetoothIntelVersionInfo * version = (BluetoothIntelVersionInfo *) mVersionInfo;
    BluetoothIntelDebugFeatures features;
    BluetoothIntelBootParams params;
    UInt32 bootAddress;
    OSData * fwData;

    if ( version->hardwarePlatform != 0x37 || (version->hardwareVariant != kBluetoothIntelHardwareVariantJfP && version->hardwareVariant != kBluetoothIntelHardwareVariantThP && version->hardwareVariant != kBluetoothIntelHardwareVariantSfP && version->hardwareVariant != kBluetoothIntelHardwareVariantWsP && version->hardwareVariant != kBluetoothIntelHardwareVariantHrP && version->hardwareVariant != kBluetoothIntelHardwareVariantCcP) )
    {
        os_log(mInternalOSLogObject, "[IntelGen2BluetoothHostController][start] This controller is not an Intel Legacy bootloader device!!!");
        return false;
    }
    
    if ( version->hardwareVariant == kBluetoothIntelHardwareVariantJfP || version->hardwareVariant == kBluetoothIntelHardwareVariantThP )
        mExpansionData->mValidLEStates = true;
    
    mExpansionData->mWidebandSpeechSupported = true;
    
    /* Setup MSFT Extension support */
    //btintel_set_msft_opcode(hdev, ver.hw_variant);
    
    /* Set the default boot parameter to 0x0 and it is updated to
     * SKU specific boot parameter after reading Intel_Write_Boot_Params
     * command while downloading the firmware.
     */
    bootAddress = 0x00000000;

    mExpansionData->mBootloaderMode = true;

    HCIRequestCreate(&id);
    err = DownloadFirmware(id, version, &params, &bootAddress);
    HCIRequestDelete(NULL, id);
    if (err)
        return false;

    /* controller is already having an operational firmware */
    if (version->firmwareVariant == 0x23)
        goto finish;

    err = BootDevice(bootAddress);
    if (err)
        return false;

    mExpansionData->mBootloaderMode = false;

    err = GetFirmware(version, &params, "ddc", &fwData);
    if (!err)
    {
        /* Once the device is running in operational mode, it needs to
         * apply the device configuration (DDC) parameters.
         *
         * The device can work without DDC parameters, so even if it
         * fails to load the file, no need to fail the setup.
         */
        HCIRequestCreate(&id);
        LoadDDCConfig(id, fwData);
        HCIRequestDelete(NULL, id);
    }

    /* Read the Intel supported features and if new exception formats
     * supported, need to load the additional DDC config to enable.
     */
    HCIRequestCreate(&id);
    err = BluetoothHCIIntelReadDebugFeatures(id, &features);
    HCIRequestDelete(NULL, id);
    if (!err)
    {
        /* Set DDC mask for available debug features */
        HCIRequestCreate(&id);
        BluetoothHCIIntelSetDebugFeatures(id, &features);
        HCIRequestDelete(NULL, id);
    }

    /* Read the Intel version information after loading the FW  */
    HCIRequestCreate(&id);
    err = BluetoothHCIIntelReadVersionInfo(id, 0x00, (UInt8 *) version);
    HCIRequestDelete(NULL, id);
    if (err)
        return false;

    PrintVersionInfo(version);

finish:
    /* Set the event mask for Intel specific vendor events. This enables
     * a few extra events that are useful during general operation. It
     * does not enable any debugging related events.
     *
     * The device will function correctly without these events enabled
     * and thus no need to fail the setup.
     */
    HCIRequestCreate(&id);
    BluetoothHCIIntelSetEventMask(id, false);
    HCIRequestDelete(NULL, id);
    
    return true;
}

IOReturn IntelGen2BluetoothHostController::GetFirmwareNameWL(void * ver, BluetoothIntelBootParams * params, const char * suffix, char * fwName)
{
    BluetoothIntelVersionInfo * version = (BluetoothIntelVersionInfo *) ver;
    char firmwareName[64];
    
    switch (version->hardwareVariant)
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

IOReturn IntelGen2BluetoothHostController::GetFirmwareWL(void * version, BluetoothIntelBootParams * params, const char * suffix, OSData ** fwData)
{
    IOReturn err;
    IntelBluetoothHostControllerUSBTransport * transport = OSDynamicCast(IntelBluetoothHostControllerUSBTransport, mBluetoothTransport);
    char fwName[64];
    
    if ( GetFirmwareName(version, params, suffix, fwName, sizeof(fwName)) )
    {
        os_log(mInternalOSLogObject, "[IntelGen2BluetoothHostController][GetFirmwareWL] Unsupported firmware name!");
        return kIOReturnInvalid;
    }
    
    if ( !transport )
    {
        os_log(mInternalOSLogObject, "[IntelGen2BluetoothHostController][GetFirmwareWL] Transport is invalid!");
        return kIOReturnInvalid;
    }
    
    err = transport->setFirmware(fwName);
    if (err)
    {
        os_log(mInternalOSLogObject, "[IntelGen2BluetoothHostController][GetFirmwareWL] Failed to obtain firmware file %s: 0x%x", fwName, err);
        return err;
    }
    *fwData = transport->getFirmware()->getFirmwareUncompressed();
    
    os_log(mInternalOSLogObject, "[IntelGen2BluetoothHostController][GetFirmwareWL] Found firmware file: %s", fwName);
    
    return kIOReturnSuccess;
}

IOReturn IntelGen2BluetoothHostController::DownloadFirmwareWL(BluetoothHCIRequestID inID, void * ver, BluetoothIntelBootParams * params, UInt32 * bootAddress)
{
    IOReturn err;
    AbsoluteTime callTime;
    BluetoothIntelVersionInfo * version = (BluetoothIntelVersionInfo *) ver;
    OSData * fwData;

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
    if ( version->firmwareVariant == 0x23 )
    {
        mExpansionData->mBootloaderMode = false;
        CheckDeviceAddress(inID);

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
    err = BluetoothHCIIntelReadBootParams(inID, params);
    if (err)
        return err;

    /* It is required that every single firmware fragment is acknowledged
     * with a command complete event. If the boot parameters indicate
     * that this bootloader does not send them, then abort the setup.
     */
    if ( params->limitedCCE != 0x00 )
    {
        os_log(mInternalOSLogObject, "[IntelGen2BluetoothHostController][DownloadFirmware] Unsupported firmware loading method: %u!", params->limitedCCE);
        return kIOReturnInvalid;
    }

    /* If the OTP has no valid Bluetooth device address, then there will
     * also be no valid address for the operational firmware.
     */
    if ( params->otpDeviceAddress.data[0] == 0 && params->otpDeviceAddress.data[1] == 0 && params->otpDeviceAddress.data[2] == 0 && params->otpDeviceAddress.data[3] == 0 && params->otpDeviceAddress.data[4] == 0 && params->otpDeviceAddress.data[5] == 0 )
    {
        os_log(mInternalOSLogObject, "[IntelGen2BluetoothHostController][DownloadFirmware] No device address configured!");
        mExpansionData->mInvalidDeviceAddress = true;
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
    
    err = GetFirmware(version, params, "sfi", &fwData);
    if ( err )
    {
        if ( !mExpansionData->mBootloaderMode )
        {
            /* Firmware has already been loaded */
            mExpansionData->mFirmwareLoaded = true;
            return kIOReturnSuccess;
        }
        return err;
    }

    if (fwData->getLength() < 644)
    {
        os_log(mInternalOSLogObject, "[IntelGen2BluetoothHostController][DownloadFirmware] Size of firmware file is invalid: %u!", fwData->getLength());
        return kIOReturnUnsupported;
    }

    callTime = mBluetoothFamily->GetCurrentTime();

    mExpansionData->mDownloading = true;

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
            /* Skip reading firmware file version in bootloader mode */
            if (version->firmwareVariant == 0x06)
                break;

            /* Skip download if firmware has the same version */
            if ( ParseFirmwareVersion(version->firmwareBuildNum, version->firmwareBuildWeek, version->firmwareBuildYear, fwData, bootAddress) )
            {
                os_log(mInternalOSLogObject, "[IntelGen2BluetoothHostController][DownloadFirmware] Firmware already loaded!");
                mExpansionData->mFirmwareLoaded = true;
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
    if (version->firmwareVariant == 0x23)
    {
        err = kIOReturnInvalid;
        goto done;
    }

    err = SecureSendSFIRSAFirmwareHeader(inID, fwData);
    if (err)
        goto done;

    err = DownloadFirmwarePayload(inID, fwData, kIntelRSAHeaderLength);
    if (err)
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
    err = WaitForFirmwareDownload(callTime, 5000);
    if (err == kIOReturnTimeout)
done:
        ResetToBootloader(inID);
    return err;
}

OSMetaClassDefineReservedUnused(IntelGen2BluetoothHostController, 0)
OSMetaClassDefineReservedUnused(IntelGen2BluetoothHostController, 1)
OSMetaClassDefineReservedUnused(IntelGen2BluetoothHostController, 2)
OSMetaClassDefineReservedUnused(IntelGen2BluetoothHostController, 3)
OSMetaClassDefineReservedUnused(IntelGen2BluetoothHostController, 4)
OSMetaClassDefineReservedUnused(IntelGen2BluetoothHostController, 5)
OSMetaClassDefineReservedUnused(IntelGen2BluetoothHostController, 6)
OSMetaClassDefineReservedUnused(IntelGen2BluetoothHostController, 7)
OSMetaClassDefineReservedUnused(IntelGen2BluetoothHostController, 8)
OSMetaClassDefineReservedUnused(IntelGen2BluetoothHostController, 9)
OSMetaClassDefineReservedUnused(IntelGen2BluetoothHostController, 10)
OSMetaClassDefineReservedUnused(IntelGen2BluetoothHostController, 11)
OSMetaClassDefineReservedUnused(IntelGen2BluetoothHostController, 12)
OSMetaClassDefineReservedUnused(IntelGen2BluetoothHostController, 13)
OSMetaClassDefineReservedUnused(IntelGen2BluetoothHostController, 14)
OSMetaClassDefineReservedUnused(IntelGen2BluetoothHostController, 15)
OSMetaClassDefineReservedUnused(IntelGen2BluetoothHostController, 16)
OSMetaClassDefineReservedUnused(IntelGen2BluetoothHostController, 17)
OSMetaClassDefineReservedUnused(IntelGen2BluetoothHostController, 18)
OSMetaClassDefineReservedUnused(IntelGen2BluetoothHostController, 19)
OSMetaClassDefineReservedUnused(IntelGen2BluetoothHostController, 20)
OSMetaClassDefineReservedUnused(IntelGen2BluetoothHostController, 21)
OSMetaClassDefineReservedUnused(IntelGen2BluetoothHostController, 22)
OSMetaClassDefineReservedUnused(IntelGen2BluetoothHostController, 23)
