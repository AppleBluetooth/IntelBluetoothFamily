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

bool IntelGen1BluetoothHostControllerUSBTransport::init(OSDictionary * dictionary)
{
    if ( !super::init() )
    {
        os_log(mInternalOSLogObject, "**** [IntelGen1BluetoothHostControllerUSBTransport][init] -- super::init() failed ****\n");
        return false;
    }
    
    mRequiredEventsQueueHead = NULL;
    mRequiredEventsQueueTail = NULL;
    mReceivedEventValid = false;
    mPatching = false;
    mIsDefaultFirmware = false;
    return true;
}

void IntelGen1BluetoothHostControllerUSBTransport::free()
{
    while ( mRequiredEventsQueueHead )
        IODelete(EventsQueueDequeue(), BluetoothHCIEventQueueNode, 1);
    super::free();
}

bool IntelGen1BluetoothHostControllerUSBTransport::start(IOService * provider)
{
    if ( !super::start(provider) )
        return false;

    mFirmwareCandidates = fwCandidates;
    mNumFirmwares = fwCount;
    setProperty("ActiveBluetoothControllerVendor", "Intel - Legacy ROM");
    return true;
}

bool IntelGen1BluetoothHostControllerUSBTransport::EventsQueueEnqueue(BluetoothHCIEventQueueNode * node)
{
    if ( !node )
        return false;
    
    if ( !mRequiredEventsQueueHead ) // queue is empty
    {
        mRequiredEventsQueueHead = node;
        mRequiredEventsQueueTail = node;
        return true;
    }
    
    mRequiredEventsQueueTail->next = node;
    mRequiredEventsQueueTail = node;
    return true;
}

BluetoothHCIEventQueueNode * IntelGen1BluetoothHostControllerUSBTransport::EventsQueueDequeue()
{
    if ( !mRequiredEventsQueueHead )
        return NULL;
    
    BluetoothHCIEventQueueNode * node = mRequiredEventsQueueHead;
    if ( !mRequiredEventsQueueHead->next )
        mRequiredEventsQueueHead = NULL;
    else
        mRequiredEventsQueueHead = mRequiredEventsQueueHead->next;
    return node;
}

void IntelGen1BluetoothHostControllerUSBTransport::ReceiveInterruptData(void * data, UInt32 dataSize, bool special)
{
    /* It ensures that the returned event matches the event data read from
     * the firmware file. At fist, it checks the length and then
     * the contents of the event.
     */
    if ( mPatching )
    {
        int location = 0;
        BluetoothHCIEventQueueNode * node;
        UInt8 * events = (UInt8 *) data;
        
        while ( mRequiredEventsQueueHead )
        {
            node = EventsQueueDequeue();
            BluetoothHCIEventPacketHeader * event = (BluetoothHCIEventPacketHeader *) (events + location);
            location += kBluetoothHCIEventPacketHeaderSize;
            if ( node->event.dataSize != event->dataSize )
            {
                os_log(mInternalOSLogObject, "**** [IntelGen1BluetoothHostControllerUSBTransport][ReceiveInterruptData] -- Event length mismatch: opCode = 0x%04X ****\n", mCurrentCommandOpCode);
                IOSafeDeleteNULL(node, BluetoothHCIEventQueueNode, 1);
                goto call_super;
            }
            if ( memcmp(events + location, node->eventParams, event->dataSize) )
            {
                os_log(mInternalOSLogObject, "**** [IntelGen1BluetoothHostControllerUSBTransport][ReceiveInterruptData] -- Event parameters mismatch: opCode = 0x%04X ****\n", mCurrentCommandOpCode);
                IOSafeDeleteNULL(node, BluetoothHCIEventQueueNode, 1);
                goto call_super;
            }
            location += event->dataSize;
            IOSafeDeleteNULL(node, BluetoothHCIEventQueueNode, 1);
        }
        mReceivedEventValid = true;
        mCommandGate->commandWakeup(&mReceivedEventValid);
    }
    
call_super:
    IOBluetoothHostControllerUSBTransport::ReceiveInterruptData(data, dataSize, special);
}

IOReturn IntelGen1BluetoothHostControllerUSBTransport::GetFirmwareNameWL(void * ver, BluetoothIntelBootParams * params, const char * suffix, char * fwName)
{
    char firmwareName[64];
    BluetoothIntelVersionInfo * version = (BluetoothIntelVersionInfo *) ver;
    
    if ( !mIsDefaultFirmware )
        snprintf(firmwareName, sizeof(firmwareName), "ibt-hw-%x.%x.%x-fw-%x.%x.%x.%x.%x.%s", version->hardwarePlatform, version->hardwareVariant, version->hardwareRevision, version->firmwareVariant, version->firmwareRevision, version->firmwareBuildNum, version->firmwareBuildWeek, version->firmwareBuildYear, suffix);
    else
        snprintf(firmwareName, sizeof(firmwareName), "ibt-hw-%x.%x.%s", version->hardwarePlatform, version->hardwareVariant, suffix);

    strcpy(fwName, firmwareName, 64);
    return kIOReturnSuccess;
}

IOReturn IntelGen1BluetoothHostControllerUSBTransport::GetFirmwareErrorHandler(void * version, BluetoothIntelBootParams * params, const char * suffix, OSData ** fwData)
{
    /* If the correct firmware patch file is not found, use the
     * default firmware patch file instead
     */
    if ( !mIsDefaultFirmware )
    {
        mIsDefaultFirmware = true;
        return GetFirmware(version, params, suffix, fwData);
    }
    return kIOReturnError;
}

IOReturn IntelGen1BluetoothHostControllerUSBTransport::PatchFirmware(OSData * fwData, UInt8 ** fwPtr, int * disablePatch)
{
    IOReturn err;
    BluetoothHCIRequestID id;
    BluetoothHCICommandPacket cmd;
    IOByteCount remain = fwData->getLength() - (*fwPtr - (UInt8 *) fwData->getBytesNoCopy());
    BluetoothHCIEventQueueNode * node;
    IntelBluetoothHostController * controller = OSDynamicCast(IntelBluetoothHostController, mBluetoothController);
    if ( !controller )
        return kIOReturnInvalid;

    mPatching = true;
    
    /* The first byte indicates the types of the patch command or event.
     * 0x01 means HCI command and 0x02 is HCI event. If the first bytes
     * in the current firmware buffer doesn't start with 0x01 or
     * the size of remain buffer is smaller than HCI command header,
     * the firmware file is corrupted and it should stop the patching
     * process.
     */
    if ( remain > kBluetoothHCICommandPacketHeaderSize && *fwPtr[0] != 0x01 )
    {
        os_log(mInternalOSLogObject, "**** [IntelGen1BluetoothHostControllerUSBTransport][PatchFirmware] -- Firmware corrupted -- invalid command read ****\n");
        mPatching = false;
        return kIOReturnInvalid;
    }
    ++(*fwPtr);
    --remain;

    cmd.opCode = *(BluetoothHCICommandOpCode *) fwPtr;
    cmd.dataSize = *(UInt8 *) (fwPtr + sizeof(BluetoothHCICommandOpCode));
    *fwPtr += kBluetoothHCICommandPacketHeaderSize;
    remain -= kBluetoothHCICommandPacketHeaderSize;

    mCurrentCommandOpCode = cmd.opCode;
    
    /* Ensure that the remain firmware data is long enough than the length
     * of command parameter. If not, the firmware file is corrupted.
     */
    if ( remain < cmd.dataSize )
    {
        os_log(mInternalOSLogObject, "**** [IntelGen1BluetoothHostControllerUSBTransport][PatchFirmware] -- Firmware corrupted -- invalid command length ****\n");
        mPatching = false;
        return kIOReturnError;
    }

    /* If there is a command that loads a patch in the firmware
     * file, then enable the patch upon success, otherwise just
     * disable the manufacturer mode, for example patch activation
     * is not required when the default firmware patch file is used
     * because there are no patch data to load.
     */
    if ( *disablePatch && (UInt16) (cmd.opCode) == 0xFC8E )
        *disablePatch = 0;

    memcpy(cmd.data, *fwPtr, cmd.dataSize);
    *fwPtr += cmd.dataSize;
    remain -= cmd.dataSize;

    /* This reads the expected events when the above command is sent to the
     * device. Some vendor commands expects more than one events, for
     * example command status event followed by vendor specific event.
     * For this case, it only keeps the last expected event. So the command
     * can be sent with __hci_cmd_sync_ev() which returns the sk_buff of
     * last expected event.
     */
    while ( remain > kBluetoothHCIEventPacketHeaderSize && *fwPtr[0] == 0x02 )
    {
        ++(*fwPtr);
        --remain;
        
        node = IONewZero(BluetoothHCIEventQueueNode, 1);
        memcpy(&node->event, *fwPtr, kBluetoothHCIEventPacketHeaderSize);
        *fwPtr += kBluetoothHCIEventPacketHeaderSize;
        remain -= kBluetoothHCIEventPacketHeaderSize;

        if ( remain < node->event.dataSize )
        {
            REQUIRE("( remain >= node->event.dataSize )");
            os_log(mInternalOSLogObject, "**** [IntelGen1BluetoothHostControllerUSBTransport][PatchFirmware] -- Firmware corrupted -- invalid event length ****\n");
            mPatching = false;
            IOSafeDeleteNULL(node, BluetoothHCIEventQueueNode, 1);
            return kIOReturnError;
        }

        node->eventParams = *fwPtr;
        if ( !node->eventParams )
        {
            REQUIRE("( node->eventParams != NULL )");
            IOSafeDeleteNULL(node, BluetoothHCIEventQueueNode, 1);
            goto INVALID_EVENT_READ;
        }
        *fwPtr += node->event.dataSize;
        remain -= node->event.dataSize;
        
        EventsQueueEnqueue(node);
    }

    /* Every HCI commands in the firmware file has its correspond event.
     * If event is not found or remain is smaller than zero, the firmware
     * file is corrupted.
     */
    if ( remain < 0 )
    {
        REQUIRE("( remain >= 0 )");
INVALID_EVENT_READ:
        os_log(mInternalOSLogObject, "**** [IntelGen1BluetoothHostControllerUSBTransport][PatchFirmware] -- Firmware corrupted -- invalid event read ****\n");
        mPatching = false;
        return kIOReturnError;
    }

    err = controller->HCIRequestCreate(&id);
    if ( err )
    {
        REQUIRE_NO_ERR(err);
        mPatching = false;
        return err;
    }
    err = controller->SendRawHCICommand(id, (char *) &cmd, cmd.dataSize + kBluetoothHCICommandPacketHeaderSize, NULL, 0);
    controller->HCIRequestDelete(NULL, id);
    if ( err )
    {
        os_log(mInternalOSLogObject, "**** [IntelGen1BluetoothHostControllerUSBTransport][PatchFirmware] ### ERROR: opCode = 0x%04X -- send request failed -- cannot dispatch patch command: 0x%x ****\n", cmd.opCode, err);
        mPatching = false;
        return err;
    }
    
    err = TransportCommandSleep(&mReceivedEventValid, 100, (char *) __FUNCTION__, true);
    mPatching = false;
    if ( err == THREAD_AWAKENED || (err == THREAD_TIMED_OUT && mReceivedEventValid) )
        mReceivedEventValid = false;
    else
        os_log(mInternalOSLogObject, "**** [IntelGen1BluetoothHostControllerUSBTransport][PatchFirmware] -- Beware! Did not receive event valid notification after waiting for 1 second, which could cause strange behavior -- this = 0x%04x ****\n", ConvertAddressToUInt32(this));
    
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
