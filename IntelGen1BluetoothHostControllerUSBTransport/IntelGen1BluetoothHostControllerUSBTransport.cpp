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

#include "IntelGen1BluetoothHostControllerUSBTransport.h"

#define super IntelBluetoothHostControllerUSBTransport
OSDefineMetaClassAndStructors(IntelGen1BluetoothHostControllerUSBTransport, super)

bool IntelGen1BluetoothHostControllerUSBTransport::start(IOService * provider)
{
    if ( !super::start(provider) )
        return false;

    IntelBluetoothHostController * controller = OSDynamicCast(IntelBluetoothHostController, mBluetoothController);
    if ( !controller )
        return false;

    IOReturn err;
    BluetoothIntelVersionInfo * version = (BluetoothIntelVersionInfo *) controller->mVersionInfo;
    OSData * fwData;
    UInt8 * fwPtr;
    BluetoothHCIRequestID id;
    int disablePatch;
    
    if ( version->hardwarePlatform != 0x37 || (version->hardwareVariant != kBluetoothIntelHardwareVariantWP && version->hardwareVariant != kBluetoothIntelHardwareVariantStP) )
    {
        os_log(mInternalOSLogObject, "[IntelGen1BluetoothHostControllerUSBTransport][start] This controller is not an Intel Legacy ROM device!!!");
        return false;
    }

    setProperty("ActiveBluetoothControllerVendor", "Intel - Legacy ROM");
    controller->mIsLegacyROMDevice = true;

    /* Apply the device specific HCI quirks
     *
     * WBS for SdP - SdP and Stp have a same hw_varaint but
     * different fw_variant
     */
    if (version->hardwareVariant == 0x08 && version->firmwareVariant == 0x22)
        controller->mWidebandSpeechSupported = true;

    /* These devices have an issue with LED which doesn't
     * go off immediately during shutdown. Set the flag
     * here to send the LED OFF command during shutdown.
     */
    controller->mBrokenLED = true;

    /* fw_patch_num indicates the version of patch the device currently
     * have. If there is no patch data in the device, it is always 0x00.
     * So, if it is other than 0x00, no need to patch the device again.
     */
    if (version->firmwarePatchVersion)
    {
        os_log(mInternalOSLogObject, "[IntelGen1BluetoothHostControllerUSBTransport][start] Device is already patched -- patch number: %02x", version->firmwarePatchVersion);
        goto complete;
    }

    /* Opens the firmware patch file based on the firmware version read
     * from the controller. If it fails to open the matching firmware
     * patch file, it tries to open the default firmware patch file.
     * If no patch file is found, allow the device to operate without
     * a patch.
     */
    GetFirmware(version, NULL, "bseq", &fwData);
    if ( !fwData )
        goto complete;
    fwPtr = (UInt8 *) fwData->getBytesNoCopy();

    /* Enable the manufacturer mode of the controller.
     * Only while this mode is enabled, the driver can download the
     * firmware patch data and configuration parameters.
     */
    
    controller->HCIRequestCreate(&id);
    err = controller->BluetoothHCIIntelEnterManufacturerMode(id);
    controller->HCIRequestDelete(NULL, id);
    if ( err )
        return false;

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
    while (fwData->getLength() > fwPtr - (UInt8 *) fwData->getBytesNoCopy())
    {
        controller->HCIRequestCreate(&id);
        err = PatchFirmware(id, fwData, &fwPtr, &disablePatch);
        controller->HCIRequestDelete(NULL, id);
        
        if (err)
        {
            /* Patching failed. Disable the manufacturer mode with reset and
             * deactivate the downloaded firmware patches.
             */
            controller->HCIRequestCreate(&id);
            err = controller->BluetoothHCIIntelExitManufacturerMode(id, kBluetoothIntelManufacturingExitResetOptionResetDeactivatePatches);
            controller->HCIRequestDelete(NULL, id);
            if (err)
                return false;

            os_log(mInternalOSLogObject, "[IntelGen1BluetoothHostControllerUSBTransport][start] Firmware patch completed and deactivated");
            goto complete;
        }
    }

    if (disablePatch)
    {
        /* Disable the manufacturer mode without reset */
        controller->HCIRequestCreate(&id);
        err = controller->BluetoothHCIIntelExitManufacturerMode(id, kBluetoothIntelManufacturingExitResetOptionsNoReset);
        controller->HCIRequestDelete(NULL, id);
        if (err)
            return false;

        os_log(mInternalOSLogObject, "[IntelGen1BluetoothHostControllerUSBTransport][start] Firmware patch completed");

        goto complete;
    }

    /* Patching completed successfully and disable the manufacturer mode
     * with reset and activate the downloaded firmware patches.
     */
    controller->HCIRequestCreate(&id);
    err = controller->BluetoothHCIIntelExitManufacturerMode(id, kBluetoothIntelManufacturingExitResetOptionResetActivatePatches);
    controller->HCIRequestDelete(NULL, id);
    if (err)
        return err;

    /* Need build number for downloaded fw patches in
     * every power-on boot
     */
    err = controller->CallBluetoothHCIIntelReadVersionInfo(0x00);
    if (err)
        return false;

    os_log(mInternalOSLogObject, "[IntelGen1BluetoothHostControllerUSBTransport][start] Firmware patch (0x%02x) completed and activated", ((BluetoothIntelVersionInfo *) controller->mVersionInfo)->firmwarePatchVersion);

complete:
    /* Set the event mask for Intel specific vendor events. This enables
     * a few extra events that are useful during general operation.
     */
    controller->HCIRequestCreate(&id);
    controller->BluetoothHCIIntelSetEventMask(id, false);
    controller->HCIRequestDelete(NULL, id);
    
    controller->HCIRequestCreate(&id);
    controller->CheckDeviceAddress(id);
    controller->HCIRequestDelete(NULL, id);
    
    return true;
}

IOReturn IntelGen1BluetoothHostControllerUSBTransport::GetFirmwareNameWL(void * ver, BluetoothIntelBootParams * params, const char * suffix, char * fwName)
{
    char firmwareName[64];
    BluetoothIntelVersionInfo * version = (BluetoothIntelVersionInfo *) ver;
    
    if (!mIsDefaultFirmware)
        snprintf(firmwareName, sizeof(firmwareName), "ibt-hw-%x.%x.%x-fw-%x.%x.%x.%x.%x.%s", version->hardwarePlatform, version->hardwareVariant, version->hardwareRevision, version->firmwareVariant, version->firmwareRevision, version->firmwareBuildNum, version->firmwareBuildWeek, version->firmwareBuildYear, suffix);
    else
        snprintf(firmwareName, sizeof(firmwareName), "ibt-hw-%x.%x.%s", version->hardwarePlatform, version->hardwareVariant, suffix);

    strcpy(fwName, firmwareName, 64);
    return kIOReturnSuccess;
}

IOReturn IntelGen1BluetoothHostControllerUSBTransport::GetFirmwareWL(void * version, BluetoothIntelBootParams * params, const char * suffix, OSData ** fwData)
{
    char fwName[64];

    if ( !mIsDefaultFirmware )
        GetFirmwareName(version, NULL, suffix, fwName, sizeof(fwName));
    else
        GetFirmwareName(version, NULL, suffix, fwName, sizeof(fwName));

    setProperty("FirmwareName", fwName);

    mFirmware = OpenFirmwareManager::withName(fwName, fwCandidates, fwCount);
    if ( !mFirmware )
    {
        os_log(mInternalOSLogObject, "[IntelGen1BluetoothHostControllerUSBTransport][GetFirmwareWL] Failed to open firmware file %s!!!", fwName);

        /* If the correct firmware patch file is not found, use the
         * default firmware patch file instead
         */
        if ( !mIsDefaultFirmware )
        {
            mIsDefaultFirmware = true;
            return GetFirmwareWL(version, params, suffix, fwData);
        }
        return kIOReturnError;
    }
    *fwData = mFirmware->getFirmwareUncompressed();

    os_log(mInternalOSLogObject, "[IntelGen1BluetoothHostControllerUSBTransport][GetFirmwareWL] Found firmware file: %s", fwName);

    return kIOReturnSuccess;
}

IOReturn IntelGen1BluetoothHostControllerUSBTransport::PatchFirmware(BluetoothHCIRequestID inID, OSData * fwData, UInt8 ** fwPtr, int * disablePatch)
{
    IOReturn err;
    BluetoothHCICommandPacket cmd;
    BluetoothHCIEventPacketHeader * event = NULL;
    UInt8 * actualEvent;
    IOByteCount actualSize;
    UInt8 * eventParam = NULL;
    IOByteCount remain = fwData->getLength() - (*fwPtr - (UInt8 *) fwData->getBytesNoCopy());
    IntelBluetoothHostController * controller = OSDynamicCast(IntelBluetoothHostController, mBluetoothController);
    if ( !controller )
        return false;

    /* The first byte indicates the types of the patch command or event.
     * 0x01 means HCI command and 0x02 is HCI event. If the first bytes
     * in the current firmware buffer doesn't start with 0x01 or
     * the size of remain buffer is smaller than HCI command header,
     * the firmware file is corrupted and it should stop the patching
     * process.
     */
    if (remain > kBluetoothHCICommandPacketHeaderSize && *fwPtr[0] != 0x01)
    {
        os_log(mInternalOSLogObject, "[IntelGen1BluetoothHostControllerUSBTransport][PatchFirmware] Firmware corrupted -- invalid command read!");
        return kIOReturnInvalid;
    }
    ++(*fwPtr);
    --remain;

    cmd.opCode = *(BluetoothHCICommandOpCode *) fwPtr;
    cmd.dataSize = *(UInt8 *) (fwPtr + sizeof(BluetoothHCICommandOpCode));
    *fwPtr += (sizeof(BluetoothHCICommandOpCode) + sizeof(UInt8));
    remain -= (sizeof(BluetoothHCICommandOpCode) + sizeof(UInt8));

    /* Ensure that the remain firmware data is long enough than the length
     * of command parameter. If not, the firmware file is corrupted.
     */
    if (remain < cmd.dataSize)
    {
        os_log(mInternalOSLogObject, "[IntelGen1BluetoothHostControllerUSBTransport][PatchFirmware] Firmware corrupted -- invalid command length!");
        return kIOReturnError;
    }

    /* If there is a command that loads a patch in the firmware
     * file, then enable the patch upon success, otherwise just
     * disable the manufacturer mode, for example patch activation
     * is not required when the default firmware patch file is used
     * because there are no patch data to load.
     */
    if (*disablePatch && (UInt16) (cmd.opCode) == 0xFC8E)
        *disablePatch = 0;

    memcpy(cmd.data, fwPtr, cmd.dataSize);
    *fwPtr += cmd.dataSize;
    remain -= cmd.dataSize;

    /* This reads the expected events when the above command is sent to the
     * device. Some vendor commands expects more than one events, for
     * example command status event followed by vendor specific event.
     * For this case, it only keeps the last expected event. so the command
     * can be sent with __hci_cmd_sync_ev() which returns the sk_buff of
     * last expected event.
     */
    while (remain > kBluetoothHCIEventPacketHeaderSize && *fwPtr[0] == 0x02)
    {
        ++(*fwPtr);
        --remain;

        event = (BluetoothHCIEventPacketHeader *)(*fwPtr);
        *fwPtr += sizeof(*event);
        remain -= sizeof(*event);

        if (remain < event->dataSize)
        {
            os_log(mInternalOSLogObject, "[IntelGen1BluetoothHostControllerUSBTransport][PatchFirmware] Firmware corrupted -- invalid event length!");
            return kIOReturnError;
        }

        eventParam = *fwPtr;
        *fwPtr += event->dataSize;
        remain -= event->dataSize;
    }

    /* Every HCI commands in the firmware file has its correspond event.
     * If event is not found or remain is smaller than zero, the firmware
     * file is corrupted.
     */
    if (!event || !eventParam || remain < 0)
    {
        os_log(mInternalOSLogObject, "[IntelGen1BluetoothHostControllerUSBTransport][PatchFirmware] Firmware corrupted -- invalid event read!");
        return kIOReturnError;
    }

    err = controller->SendRawHCICommand(inID, (char *) &cmd, cmd.dataSize + kBluetoothHCICommandPacketHeaderSize, NULL, 0);
    if (err)
    {
        os_log(mInternalOSLogObject, "[IntelGen1BluetoothHostControllerUSBTransport][PatchFirmware] ### ERROR: opCode = 0x%04X -- send request failed -- cannot dispatch patch command: 0x%x", cmd.opCode, err);
        return kIOReturnError;
    }
     
    /* It ensures that the returned event matches the event data read from
     * the firmware file. At fist, it checks the length and then
     * the contents of the event.
     */

    actualSize = mInterruptReadDataBuffer->getLength(); // not sure though
    actualEvent = (UInt8 *) mInterruptReadDataBuffer->getBytesNoCopy();
    if ( event->dataSize != actualSize )
    {
        os_log(mInternalOSLogObject, "[IntelGen1BluetoothHostControllerUSBTransport][PatchFirmware] Event length mismatch: opCode = 0x%04X", cmd.opCode);
        return kIOReturnError;
    }
    if (memcmp(actualEvent, eventParam, actualSize))
    {
        os_log(mInternalOSLogObject, "[IntelGen1BluetoothHostControllerUSBTransport][PatchFirmware] Event parameters mismatch: opCode = 0x%04X", cmd.opCode);
        return kIOReturnError;
    }
    
    return kIOReturnSuccess;
}

OSMetaClassDefineReservedUnused(IntelGen1BluetoothHostControllerUSBTransport, 0)
OSMetaClassDefineReservedUnused(IntelGen1BluetoothHostControllerUSBTransport, 1)
OSMetaClassDefineReservedUnused(IntelGen1BluetoothHostControllerUSBTransport, 2)
OSMetaClassDefineReservedUnused(IntelGen1BluetoothHostControllerUSBTransport, 3)
OSMetaClassDefineReservedUnused(IntelGen1BluetoothHostControllerUSBTransport, 4)
OSMetaClassDefineReservedUnused(IntelGen1BluetoothHostControllerUSBTransport, 5)
OSMetaClassDefineReservedUnused(IntelGen1BluetoothHostControllerUSBTransport, 6)
OSMetaClassDefineReservedUnused(IntelGen1BluetoothHostControllerUSBTransport, 7)
OSMetaClassDefineReservedUnused(IntelGen1BluetoothHostControllerUSBTransport, 8)
OSMetaClassDefineReservedUnused(IntelGen1BluetoothHostControllerUSBTransport, 9)
OSMetaClassDefineReservedUnused(IntelGen1BluetoothHostControllerUSBTransport, 10)
OSMetaClassDefineReservedUnused(IntelGen1BluetoothHostControllerUSBTransport, 11)
OSMetaClassDefineReservedUnused(IntelGen1BluetoothHostControllerUSBTransport, 12)
OSMetaClassDefineReservedUnused(IntelGen1BluetoothHostControllerUSBTransport, 13)
OSMetaClassDefineReservedUnused(IntelGen1BluetoothHostControllerUSBTransport, 14)
OSMetaClassDefineReservedUnused(IntelGen1BluetoothHostControllerUSBTransport, 15)
OSMetaClassDefineReservedUnused(IntelGen1BluetoothHostControllerUSBTransport, 16)
OSMetaClassDefineReservedUnused(IntelGen1BluetoothHostControllerUSBTransport, 17)
OSMetaClassDefineReservedUnused(IntelGen1BluetoothHostControllerUSBTransport, 18)
OSMetaClassDefineReservedUnused(IntelGen1BluetoothHostControllerUSBTransport, 19)
OSMetaClassDefineReservedUnused(IntelGen1BluetoothHostControllerUSBTransport, 20)
OSMetaClassDefineReservedUnused(IntelGen1BluetoothHostControllerUSBTransport, 21)
OSMetaClassDefineReservedUnused(IntelGen1BluetoothHostControllerUSBTransport, 22)
OSMetaClassDefineReservedUnused(IntelGen1BluetoothHostControllerUSBTransport, 23)
