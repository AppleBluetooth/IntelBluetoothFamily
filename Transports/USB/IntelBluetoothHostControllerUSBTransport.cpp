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

#include "IntelBluetoothHostControllerUSBTransport.h"
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/bluetooth/IOBluetoothMemoryBlock.h>

static IOPMPowerState powerStateArray[kIOBluetoothHCIControllerPowerStateOrdinalCount] =
{
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {1, kIOPMLowPower, kIOPMPowerOn, kIOPMLowPower, 0, 0, 0, 0, 0, 0, 0, 0},
    {1, kIOPMDeviceUsable, kIOPMPowerOn, kIOPMPowerOn, 0, 0, 0, 0, 0, 0, 0, 0}
};

#define super IOBluetoothHostControllerUSBTransport
OSDefineMetaClassAndStructors(IntelBluetoothHostControllerUSBTransport, super)

bool IntelBluetoothHostControllerUSBTransport::init(OSDictionary * dictionary)
{
    CreateOSLogObject();
    if ( !super::init() )
    {
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostControllerUSBTransport][init] -- super::init() failed ****\n");
        return false;
    }
    
    mExpansionData = IONewZero(ExpansionData, 1);
    if ( !mExpansionData )
        return false;
    
    mFirmware = NULL;

    return true;
}

void IntelBluetoothHostControllerUSBTransport::free()
{
    IOSafeDeleteNULL(mExpansionData, ExpansionData, 1);
    if ( mFirmware )
        mFirmware->removeFirmwares();
    OSSafeReleaseNULL(mFirmware);

    super::free();
}

IOService * IntelBluetoothHostControllerUSBTransport::probe(IOService * provider, SInt32 * score)
{
    IOService * result = super::probe(provider, score);
    IOUSBHostDevice * device;
    UInt16 vendorID;
    UInt16 productID;

    device = OSDynamicCast(IOUSBHostDevice, provider);
    if ( !device )
        return NULL;

    vendorID = device->getDeviceDescriptor()->idVendor;
    productID = device->getDeviceDescriptor()->idProduct;
    os_log(mInternalOSLogObject, "**** [IntelBluetoothHostControllerUSBTransport][probe] -- USB device info: name: %s, vendor ID: 0x%04X, product ID: 0x%04X\n", device->getName(), vendorID, productID);

    return result;
}

bool IntelBluetoothHostControllerUSBTransport::start(IOService * provider)
{
    if ( !super::start(provider) )
        return false;

    if ( mVendorID == 0x8087 )
    {
        mControllerVendorType = 8;
        setProperty("ActiveBluetoothControllerVendor", "Intel");
    }
    else
        return false;

    mBluetoothUSBHostDevice->retain();
    registerService();

    return true;
}

void IntelBluetoothHostControllerUSBTransport::stop(IOService * provider)
{
    IOReturn err;
    BluetoothHCIRequestID id;

    IntelBluetoothHostController * controller = OSDynamicCast(IntelBluetoothHostController, mBluetoothController);
    if ( !controller )
        return;

    /* Send HCI Reset to the controller to stop any BT activity which
     * were triggered. This will help to save power and maintain the
     * sync b/w Host and controller
     */
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_11_0
    if ( controller->CallBluetoothHCIReset(false, (char *) __FUNCTION__) )
#else
    if ( controller->CallBluetoothHCIReset(false) )
#endif
        return;

    /* Some platforms have an issue with BT LED when the interface is
     * down or BT radio is turned off, which takes 5 seconds to BT LED
     * goes off. This command turns off the BT LED immediately.
     */
    if ( controller->mBrokenLED )
    {
        err = controller->HCIRequestCreate(&id);
        if ( err )
        {
            REQUIRE_NO_ERR(err);
            return;
        }
        controller->BluetoothHCIIntelTurnOffDeviceLED(id);
        controller->HCIRequestDelete(NULL, id);
    }
    super::stop(provider);
}

bool IntelBluetoothHostControllerUSBTransport::InitializeTransportWL(IOService * provider)
{
    if ( !provider )
        return false;

    IOService * platformProvider;
    OSNumber * maxPower;

    platformProvider = getPlatform()->getProvider();
    if ( platformProvider )
    {
        maxPower = OSDynamicCast(OSNumber, platformProvider->getProperty("bt-maxpower"));
        if ( maxPower != NULL )
        {
            if ( !mBluetoothController )
                return false;
            mBluetoothController->mMaxPower = maxPower->unsigned16BitValue();
        }
    }

    if ( super::InitializeTransportWL(provider) )
    {
        SetRemoteWakeUp(true);
        mHardwareInitialized = true;
        return true;
    }

    mHardwareInitialized = false;
    return false;
}

IOReturn IntelBluetoothHostControllerUSBTransport::SendHCIRequest(UInt8 * buffer, IOByteCount size)
{
    IOReturn err;

    if ( !buffer || size < kBluetoothHCICommandPacketHeaderSize )
        return kIOReturnInvalid;

    if ( *(UInt16 *) buffer == 0xFC09 && mBluetoothController && ((IntelBluetoothHostController *) mBluetoothController)->mBootloaderMode )
    {
        err = TransportSecureSendBulkOutWrite(buffer, (UInt32) size);
        if ( err )
        {
            REQUIRE_NO_ERR(err);
            return err;
        }
        if ( PostSecureSendBulkPipeRead() )
            return kIOReturnSuccess;
        return kIOReturnError;
    }

    err = super::SendHCIRequest(buffer, size);
    if ( err )
        return err;

    if ( *(UInt16 *) buffer == 0xFC01 )
        InjectCommandCompleteEvent(0xFC01);

    return kIOReturnSuccess;
}

void IntelBluetoothHostControllerUSBTransport::InjectCommandCompleteEvent(BluetoothHCICommandOpCode opCode)
{
    UInt8 * packet;
    UInt32 packetSize;

    packetSize = kBluetoothHCIEventPacketHeaderSize + sizeof(BluetoothHCIEventCommandCompleteResults) + 1;
    packet = IONewZero(UInt8, packetSize);
    if ( !packet )
    {
        REQUIRE("( packet != NULL )");
        return;
    }

    BluetoothHCIEventPacketHeader * header = (BluetoothHCIEventPacketHeader *) packet;
    header->eventCode = kBluetoothHCIEventCommandComplete;
    header->dataSize  = sizeof(BluetoothHCIEventCommandCompleteResults) + 1;

    BluetoothHCIEventCommandCompleteResults * event = (BluetoothHCIEventCommandCompleteResults *) (packet + kBluetoothHCIEventPacketHeaderSize);
    event->numCommands = 0x01;
    event->opCode = opCode;

    packet[packetSize - 1] = 0x00;

    ReceiveInterruptData(packet, packetSize, false);
}

bool IntelBluetoothHostControllerUSBTransport::PostSecureSendBulkPipeRead()
{
    IOReturn err;
    IOUSBHostCompletion completion;
    char errStrLong[100];
    char errStrShort[50];

    if ( !mBulkInPipe )
        return false;

    err = mBulkInPipe->clearStall(true);
    if ( err )
    {
        mBluetoothFamily->ConvertErrorCodeToString(err, errStrLong, errStrShort);
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostControllerUSBTransport][PostSecureSendBulkPipeRead] -- ERROR -- clearStall (true) failed with error 0x%04X (%s) -- 0x%04x\n", err, errStrLong, ConvertAddressToUInt32(this));
        BluetoothFamilyLogPacket(mBluetoothFamily, 250, "SecureSend - clearStall 0x%04X %s", err, errStrShort);
        return false;
    }

    completion.owner 	 = this;
    completion.action 	 = IntelBluetoothHostControllerUSBTransport::SecureSendBulkInReadHandler;
    completion.parameter = NULL;

    RetainTransport((char *) __FUNCTION__);

    err = mBulkInPipe->io(mBulkInReadDataBuffer, 1021, &completion);
    if ( !err )
    {
        ++mBulkInPipeOutstandingIOCount;
        return true;
    }

    ReleaseTransport((char *) __FUNCTION__);

    mBluetoothFamily->ConvertErrorCodeToString(err, errStrLong, errStrShort);
    os_log(mInternalOSLogObject, "**** [IntelBluetoothHostControllerUSBTransport][PostSecureSendBulkPipeRead] -- ERROR -- failed to read on the bulk in pipe: 0x%04X (%s) -- this = 0x%04x\n", err, errStrLong, ConvertAddressToUInt32(this));
    BluetoothFamilyLogPacket(mBluetoothFamily, 250, "SecureSend - Read 0x%04X %s", err, errStrShort);
    return false;
}

void IntelBluetoothHostControllerUSBTransport::SecureSendBulkInReadHandler(void * owner, void * parameter, IOReturn inStatus, uint32_t dataSize)
{
    IntelBluetoothHostControllerUSBTransport * that = (IntelBluetoothHostControllerUSBTransport *) owner;
    char errStrLong[100];
    char errStrShort[50];

    --that->mBulkInPipeOutstandingIOCount;

    that->mBluetoothFamily->ConvertErrorCodeToString(inStatus, errStrLong, errStrShort);

    if ( inStatus && dataSize )
        BluetoothFamilyLogPacket(that->mBluetoothFamily, 250, "**** [IntelBluetoothHostControllerUSBTransport][SecureSendBulkInReadHandler] -- inStatus = 0x%04X (%s), dataSize = %u ****\n", inStatus, errStrLong, dataSize);

    if ( !that->mBulkInPipe )
    {
        that->mBulkInPipeStarted = false;
        os_log(that->mInternalOSLogObject, "**** [IntelBluetoothHostControllerUSBTransport][SecureSendBulkInReadHandler] -- Pipe Is Gone\n");
        BluetoothFamilyLogPacket(that->mBluetoothFamily, 250, "Bulk In Pipe Gone");
FAIL:
        that->ReleaseTransport((char *) __FUNCTION__);
        return;
    }

    if ( that->mBluetoothController && !that->mBluetoothController->mHardResetPerformed && that->mBluetoothFamily->mTestNotRespondingHardReset && (that->mBluetoothFamily->GetCurrentTime() - that->mBluetoothFamily->mUSBHardResetWLCallTime) >= 0x3938701 )
    {
        os_log(that->mInternalOSLogObject, "**** [IntelBluetoothHostControllerUSBTransport][SecureSendBulkInReadHandler] -- mBluetoothFamily->mTestNotRespondingHardReset is true ****");
        if ( that->mBuiltIn )
        {
            that->mHardResetState = 2;
            that->mBluetoothController->mHardResetPerformed = true;
            os_log(that->mInternalOSLogObject, "**** [IntelBluetoothHostControllerUSBTransport][SecureSendBulkInReadHandler] -- calling HardReset() ****");
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_10_15
            that->HardReset();
#else
            that->PerformHardReset();
#endif
        }
    }

    if ( !inStatus )
    {
        that->mBulkInReadNumRetries = 0;
        that->ReceiveInterruptData(that->mBulkInReadDataBuffer->getBytesNoCopy(), dataSize, false);
        return;
    }

    if ( inStatus == kIOReturnAborted )
    {
        if ( that->mBulkInPipeOutstandingIOCount )
            OSLogAndLogPacket(that->mInternalOSLogObject, that->mBluetoothFamily, 250, "**** [IntelBluetoothHostControllerUSBTransport][SecureSendBulkInReadHandler] -- kIOReturnAborted -- Number of outstanding IO on the Bulk In Pipe (%d) > 0 \n", that->mBulkInPipeOutstandingIOCount);
        else
            BluetoothFamilyLogPacket(that->mBluetoothFamily, 249, "**** [IntelBluetoothHostControllerUSBTransport][SecureSendBulkInReadHandler] -- kIOReturnAborted -- Number of outstanding IO on the Bulk In Pipe = %d \n", 0);

        BluetoothFamilyLogPacket(that->mBluetoothFamily, 248, "Bulk In Pipe: Aborted");
        BluetoothFamilyLogPacket(that->mBluetoothFamily, 249, "**** [IntelBluetoothHostControllerUSBTransport][SecureSendBulkInReadHandler] -- kIOReturnAborted -- dataSize = %u -- inTarget = 0x%04x ****\n", dataSize, that->ConvertAddressToUInt32(that));

        if ( dataSize )
        {
            BluetoothFamilyLogPacket(that->mBluetoothFamily, 249, "**** [IntelBluetoothHostControllerUSBTransport][SecureSendBulkInReadHandler] -- kIOReturnAborted but dataSize (%u) is non zero -- calling ReceiveInterruptData() ****\n", dataSize);
            that->ReceiveInterruptData(that->mBulkInReadDataBuffer->getBytesNoCopy(), dataSize, false);
        }
        else if ( bcmp(that->mEmptyInterruptReadData, that->mBulkInReadDataBuffer->getBytesNoCopy(), 0x3FD) )
        {
            BluetoothFamilyLogPacket(that->mBluetoothFamily, 249, "**** [IntelBluetoothHostControllerUSBTransport][SecureSendBulkInReadHandler] -- kIOReturnAborted and dataSize is zero but contains data -- calling ReceiveInterruptData() ****\n");
            that->ReceiveInterruptData(that->mBulkInReadDataBuffer->getBytesNoCopy(), 0, true);
        }

        if ( (that->isInactive() || that->mBulkInReadNumRetries >= 5) && !that->TransportWillReEnumerate() )
        {
            os_log(that->mInternalOSLogObject, "**** [IntelBluetoothHostControllerUSBTransport][SecureSendBulkInReadHandler] -- Received kIOReturnAborted error - no more retries - bailing out -- 0x%04x ****\n", that->ConvertAddressToUInt32(that));
            goto ABORT;
        }

        ++that->mInterruptReadNumRetries;
        os_log(that->mInternalOSLogObject, "**** [IntelBluetoothHostControllerUSBTransport][SecureSendBulkInReadHandler] -- Received kIOReturnAborted error - retrying: %d -- 0x%04x\n", that->mInterruptReadNumRetries, that->ConvertAddressToUInt32(that));
        return;
    }

    BluetoothFamilyLogPacket(that->mBluetoothFamily, 250, "Bulk In Read %s", errStrShort);

    if ( inStatus == kIOReturnNotResponding || inStatus == kUSBHostReturnPipeStalled )
    {
        os_log(that->mInternalOSLogObject, "**** [IntelBluetoothHostControllerUSBTransport][SecureSendBulkInReadHandler] -- kIOReturnNotResponding or kUSBHostReturnPipeStalled -- dataSize = %u -- inTarget = 0x%04x ****\n", dataSize, that->ConvertAddressToUInt32(that));

        if ( that->mBluetoothController )
        {
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_10_14
            that->mBluetoothController->BroadcastNotification(11, kIOBluetoothHCIControllerConfigStateUninitialized, kIOBluetoothHCIControllerConfigStateUninitialized);
#else
            that->mBluetoothController->BroadcastConfigStateChangeNotification(kIOBluetoothHCIControllerConfigStateUninitialized, kIOBluetoothHCIControllerConfigStateUninitialized);
#endif
        }

        if ( inStatus == kIOReturnNotResponding )
        {
            os_log(that->mInternalOSLogObject, "**** [IntelBluetoothHostControllerUSBTransport][SecureSendBulkInReadHandler] -- kIOReturnNotResponding -- dataSize = %u -- inTarget = 0x%04x ****\n", dataSize, that->ConvertAddressToUInt32(that));
            if ( dataSize )
            {
                BluetoothFamilyLogPacket(that->mBluetoothFamily, 249, "**** [IntelBluetoothHostControllerUSBTransport][SecureSendBulkInReadHandler] -- kIOReturnNotResponding -- but dataSize (%u) is non zero -- calling ReceiveInterruptData() ****\n", dataSize);
                that->ReceiveInterruptData(that->mBulkInReadDataBuffer->getBytesNoCopy(), dataSize, false);
                if ( that->mStopInterruptPipeReadCounter )
                    --that->mStopInterruptPipeReadCounter;
            }
            else if ( bcmp(that->mEmptyInterruptReadData, that->mBulkInReadDataBuffer->getBytesNoCopy(), 0x3FD) )
            {
                BluetoothFamilyLogPacket(that->mBluetoothFamily, 249, "**** [IntelBluetoothHostControllerUSBTransport][SecureSendBulkInReadHandler] -- kIOReturnNotResponding and dataSize is zero but contains data -- calling ReceiveInterruptData() ****\n");
                that->ReceiveInterruptData(that->mInterruptReadDataBuffer->getBytesNoCopy(), 0, true);
            }
        }

        if ( that->TerminateCalled() || that->isInactive() || that->mStopAllPipesCalled || that->mBluetoothController->mHardResetPerformed )
            goto ABORT;

        ++that->mInterruptReadNumRetries;
        if ( that->mInterruptReadNumRetries >= 5 )
        {
            OSLogAndLogPacket(that->mInternalOSLogObject, that->mBluetoothFamily, 249, "**** [IntelBluetoothHostControllerUSBTransport][SecureSendBulkInReadHandler] -- Received %s error - no more retries \n", errStrLong);
            BluetoothFamilyLogPacket(that->mBluetoothFamily, 250, "Bulk In Read - no more retry");

            if ( that->mBluetoothFamily )
                *(UInt8 *)(((UInt8 *) that->mBluetoothController) + 969) = 0;

            if ( !that->mBluetoothController->mHardResetPerformed && that->mBuiltIn )
            {
                that->mHardResetState = 2;
                that->mBluetoothController->mHardResetPerformed = 1;
                BluetoothFamilyLogPacket(that->mBluetoothFamily, 248, "Bulk In Read -- Hardware Reset");
                os_log(that->mInternalOSLogObject, "**** [IntelBluetoothHostControllerUSBTransport][SecureSendBulkInReadHandler] -- calling HardReset()\n");
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_10_15
                that->HardReset();
#else
                that->PerformHardReset();
#endif
            }
            goto ABORT;
        }
        return;
    }

ABORT:
    if ( that->mStopAllPipesCalled )
    {
        if ( that->mBulkInPipeOutstandingIOCount )
            OSLogAndLogPacket(that->mInternalOSLogObject, that->mBluetoothFamily, 250, "**** [IntelBluetoothHostControllerUSBTransport][SecureSendBulkInReadHandler] -- Bulk In Pipe Aborted -- Number of outstanding IO on the Bulk In Pipe (%d) > 0 \n", that->mBulkInPipeOutstandingIOCount);
        else
            BluetoothFamilyLogPacket(that->mBluetoothFamily, 249, "**** [IntelBluetoothHostControllerUSBTransport][SecureSendBulkInReadHandler] -- Bulk In Pipe Aborted -- Number of outstanding IO on the Bulk In Pipe = %d \n", that->mBulkInPipeOutstandingIOCount);

        BluetoothFamilyLogPacket(that->mBluetoothFamily, 248, "Bulk In: IO %d", that->mBulkInPipeOutstandingIOCount);
    }

    if ( inStatus == kIOReturnAborted && !that->mInterruptPipeOutstandingIOCount && that->mCommandGate )
        that->mCommandGate->commandWakeup(&that->mInterruptPipeOutstandingIOCount);
    goto FAIL;
}

IOReturn IntelBluetoothHostControllerUSBTransport::TransportSecureSendBulkOutWrite(UInt8 * buffer, UInt32 size)
{
    IOReturn err;
    IOMemoryDescriptor * md = IOMemoryDescriptor::withAddress(buffer, size, kIODirectionOut);
    if ( !md )
        return -536870211;
    err = SecureSendBulkOutWrite(md);
    OSSafeReleaseNULL(md);
    return err;
}

IOReturn IntelBluetoothHostControllerUSBTransport::SecureSendBulkOutWrite(IOMemoryDescriptor * memDescriptor)
{
    IOReturn err;
    uint32_t bytesTransferred = 0;
    char errStrLong[100];
    char errStrShort[50];

    if ( !mBulkOutPipe )
        return -536870208;

    if ( !memDescriptor )
        return -536870206;

    if ( memDescriptor->prepare(kIODirectionOut) )
        return -536870211;

    if ( mCurrentInternalPowerState == kIOBluetoothHCIControllerInternalPowerStateOn )
        err = mBulkOutPipe->io(memDescriptor, (UInt32) memDescriptor->getLength(), bytesTransferred, 2000);
    else
    {
        OSLogAndLogPacket(mInternalOSLogObject, mBluetoothFamily, 250, "Error -- trying to call mBulkOutPipe->io() when power is not ON");
        err = -536870173;
    }

    memDescriptor->complete(kIODirectionOut);

    if ( !err )
        return kIOReturnSuccess;

    mBluetoothFamily->ConvertErrorCodeToString(err, errStrLong, errStrShort);
    os_log(mInternalOSLogObject, "**** [IntelBluetoothHostControllerUSBTransport][SecureSendBulkOutWrite] -- mBulkOutPipe->io() failed: 0x%04X (%s) -- 0x%04x ****\n", err, errStrLong, ConvertAddressToUInt32(this));
    BluetoothFamilyLogPacket(mBluetoothFamily, 250, "mBulkOutPipe->io() 0x%04X %s", err, errStrShort);
    return err;
}

bool IntelBluetoothHostControllerUSBTransport::SupportNewIdlePolicy()
{
    mSupportNewIdlePolicy = true;
    return setProperty("SupportNewIdlePolicy", true);
}

IOReturn IntelBluetoothHostControllerUSBTransport::CallPowerManagerChangePowerStateTo(unsigned long ordinal, char * name)
{
    if ( mBluetoothController )
    {
        mBluetoothController->SetChangingPowerState(true);
        if ( mSupportNewIdlePolicy )
            mBluetoothController->ChangeIdleTimerTime((char *) __FUNCTION__, mBluetoothController->mNewIdleTime);
    }
    mCurrentPMMethod = 4;

    switch (ordinal)
    {
        case kIOBluetoothHCIControllerPowerStateOrdinalOff:
            return changePowerStateToPriv(0);

        case kIOBluetoothHCIControllerPowerStateOrdinalOn:
            return changePowerStateToPriv(1);

        default:
            return kIOPMNoErr;
    }
}

bool IntelBluetoothHostControllerUSBTransport::ConfigurePM(IOService * policyMaker)
{
    IOService * provider;
    int err;
    uint8_t i;

    if ( !mBluetoothUSBHostDevice )
      goto CONFIG_PM;

    provider = mBluetoothUSBHostDevice->getProvider();
    if ( !provider )
        return false;

    if ( provider->getProvider()->getProvider() )
        mBluetoothUSBHub = OSDynamicCast(IOUSBHostDevice, provider->getProvider()->getProvider());

CONFIG_PM:
    if ( !pm_vars )
    {
        PMinit();
        policyMaker->joinPMtree(this);
        if ( !pm_vars )
            return false;
    }

    mSupportPowerOff = true;
    registerPowerDriver(this, powerStateArray, kIOBluetoothHCIControllerPowerStateOrdinalCount);
    setProperty("SupportPowerOff", true);

    if ( mBluetoothFamily )
    {
        mBluetoothFamily->setProperty("TransportType", "USB");
        mConrollerTransportType = kBluetoothTransportTypeUSB;
    }

    if ( !mConfiguredPM && mCommandGate )
    {
        for (i = 0; i < 100; ++i)
        {
            err = TransportCommandSleep(&mConfiguredPM, 300, (char *) __FUNCTION__, true);

            if ( isInactive() || mConfiguredPM )
                goto OVER;

            if ( err == THREAD_AWAKENED || (err == THREAD_TIMED_OUT && mConfiguredPM) )
                goto OVER;
        }
        os_log(mInternalOSLogObject, "**** [IOBluetoothHostControllerUSBTransport][ConfigurePM] -- ERROR -- waited 30 seconds and still did not get the commandWakeup() notification -- 0x%04x ****\n", ConvertAddressToUInt32(this));
        mConfiguredPM = true;
    }

OVER:
    BluetoothFamilyLogPacket(mBluetoothFamily, 251, "USB Low Power");
    changePowerStateTo(1);
    ReadyToGo(mConfiguredPM);

    SetRadioPowerState(kRadioPoweredOn);
    return true;
}

void IntelBluetoothHostControllerUSBTransport::systemWillShutdownWL(IOOptionBits options, void * parameter)
{
    if ( !mCurrentInternalPowerState )
        return;

    if ( options == kIOMessageSystemWillRestart )
    {
        AbortPipesAndClose(true, true);
        terminate();
    }

    super::systemWillShutdownWL(options, parameter);
}

bool IntelBluetoothHostControllerUSBTransport::ControllerSupportWoBT()
{
    mSupportWoBT = true;
    return true;
}

UInt8 IntelBluetoothHostControllerUSBTransport::GetRadioPowerState()
{
    return mRadioPowerState;
}

void IntelBluetoothHostControllerUSBTransport::SetRadioPowerState(UInt8 state)
{
    mRadioPowerState = state;
}

bool IntelBluetoothHostControllerUSBTransport::SearchForUSBCompositeDriver(IOUSBHostDevice * device)
{
    OSIterator * clientIterator;
    OSObject * obj;
    IOService * service;
    OSString * ioClass;

    if ( !device )
        return false;

    clientIterator = device->getClientIterator();
    if ( !clientIterator )
        return false;

    obj = clientIterator->getNextObject();
    if ( !obj )
    {
        OSSafeReleaseNULL(clientIterator);
        return false;
    }

    while ( obj )
    {
        service = OSDynamicCast(IOService, obj);
        if ( service && service->getProperty("IOClass") )
        {
            ioClass = OSDynamicCast(OSString, service->getProperty("IOClass"));
            if ( ioClass && ioClass->isEqualTo("AppleUSBHostCompositeDevice") )
            {
                OSSafeReleaseNULL(clientIterator);
                return true;
            }
        }
        obj = clientIterator->getNextObject();
    }
    OSSafeReleaseNULL(clientIterator);
    return false;
}

bool IntelBluetoothHostControllerUSBTransport::CompositeDeviceAppears()
{
    OSIterator * clientIterator;
    OSObject * obj;
    IOUSBHostDevice * device;
    const StandardUSB::DeviceDescriptor * desc;

    if ( !mBluetoothUSBHostDevice || !getProvider() || !getProvider()->getProvider() )
        return false;

    clientIterator = getProvider()->getProvider()->getClientIterator();
    if ( !clientIterator )
        return false;

    obj = clientIterator->getNextObject();
    if ( !obj )
    {
        OSSafeReleaseNULL(clientIterator);
        return false;
    }

    while ( obj )
    {
        device = OSDynamicCast(IOUSBHostDevice, obj);
        if ( !device )
        {
            obj = clientIterator->getNextObject();
            continue;
        }

        desc = device->getDeviceDescriptor();
        if ( desc && desc->idVendor == 0x8087 )
        {
            OSSafeReleaseNULL(clientIterator);
            return true;
        }
    }

    OSSafeReleaseNULL(clientIterator);
    return false;
}

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_10_14
bool IntelBluetoothHostControllerUSBTransport::NeedToTurnOnUSBDebug()
{
    return false;
}
#endif

IOReturn IntelBluetoothHostControllerUSBTransport::GetFirmwareName(void * version, BluetoothIntelBootParams * params, const char * suffix, char * fwName, IOByteCount size)
{
    return mCommandGate->runAction(GetFirmwareNameAction, version, params, (void *) suffix, fwName);
}

IOReturn IntelBluetoothHostControllerUSBTransport::GetFirmwareNameAction(OSObject * owner, void * arg0, void * arg1, void * arg2, void * arg3)
{
    IntelBluetoothHostControllerUSBTransport * object = OSDynamicCast(IntelBluetoothHostControllerUSBTransport, owner);
    return object->GetFirmwareNameWL(arg0, (BluetoothIntelBootParams *) arg1, (const char *) arg2, (char *) arg3);
}

IOReturn IntelBluetoothHostControllerUSBTransport::GetFirmwareNameWL(void * version, BluetoothIntelBootParams * params, const char * suffix, char * fwName)
{
    return kIOReturnUnsupported;
}

IOReturn IntelBluetoothHostControllerUSBTransport::GetFirmware(void * version, BluetoothIntelBootParams * params, const char * suffix, OSData ** fwData)
{
    char fwName[64];

    if ( GetFirmwareName(version, params, suffix, fwName, sizeof(fwName)) )
    {
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostControllerUSBTransport][GetFirmware] -- Unsupported firmware name! ****\n");
        return kIOReturnInvalid;
    }

    setProperty("FirmwareName", fwName);

    mFirmware = OpenFirmwareManager::withName(fwName, mFirmwareCandidates, mNumFirmwares);
    if ( !mFirmware )
    {
        os_log(mInternalOSLogObject, "**** [IntelBluetoothHostControllerUSBTransport][GetFirmware] -- Failed to obtain firmware file %s! ****\n", fwName);
        return GetFirmwareErrorHandler(version, params, suffix, fwData);
    }

    *fwData = mFirmware->getFirmwareUncompressed(fwName);

    os_log(mInternalOSLogObject, "**** [IntelBluetoothHostControllerUSBTransport][GetFirmware] -- Found firmware file: %s ****\n", fwName);

    return kIOReturnSuccess;
}

IOReturn IntelBluetoothHostControllerUSBTransport::GetFirmwareErrorHandler(void * version, BluetoothIntelBootParams * params, const char * suffix, OSData ** fwData)
{
    return kIOReturnUnsupported;
}

IOReturn IntelBluetoothHostControllerUSBTransport::PatchFirmware(OSData * fwData, UInt8 ** fwPtr, int * disablePatch)
{
    return kIOReturnUnsupported;
}

IOReturn IntelBluetoothHostControllerUSBTransport::DownloadFirmware(void * version, BluetoothIntelBootParams * params, UInt32 * bootAddress)
{
    return mCommandGate->runAction(DownloadFirmwareAction, version, params, bootAddress);
}

IOReturn IntelBluetoothHostControllerUSBTransport::DownloadFirmwareAction(OSObject * owner, void * arg0, void * arg1, void * arg2, void * arg3)
{
    IntelBluetoothHostControllerUSBTransport * object = OSDynamicCast(IntelBluetoothHostControllerUSBTransport, owner);
    return object->DownloadFirmwareWL(arg0, (BluetoothIntelBootParams *) arg1, (UInt32 *) arg2);
}

IOReturn IntelBluetoothHostControllerUSBTransport::DownloadFirmwareWL(void * version, BluetoothIntelBootParams * params, UInt32 * bootAddress)
{
    return kIOReturnUnsupported;
}

IOReturn IntelBluetoothHostControllerUSBTransport::ParseVersionInfoTLV(BluetoothIntelVersionInfoTLV * version, UInt8 * data, IOByteCount dataSize)
{
    return kIOReturnUnsupported;
}

OSMetaClassDefineReservedUnused(IntelBluetoothHostControllerUSBTransport, 0)
OSMetaClassDefineReservedUnused(IntelBluetoothHostControllerUSBTransport, 1)
OSMetaClassDefineReservedUnused(IntelBluetoothHostControllerUSBTransport, 2)
OSMetaClassDefineReservedUnused(IntelBluetoothHostControllerUSBTransport, 3)
OSMetaClassDefineReservedUnused(IntelBluetoothHostControllerUSBTransport, 4)
OSMetaClassDefineReservedUnused(IntelBluetoothHostControllerUSBTransport, 5)
OSMetaClassDefineReservedUnused(IntelBluetoothHostControllerUSBTransport, 6)
OSMetaClassDefineReservedUnused(IntelBluetoothHostControllerUSBTransport, 7)
OSMetaClassDefineReservedUnused(IntelBluetoothHostControllerUSBTransport, 8)
OSMetaClassDefineReservedUnused(IntelBluetoothHostControllerUSBTransport, 9)
OSMetaClassDefineReservedUnused(IntelBluetoothHostControllerUSBTransport, 10)
OSMetaClassDefineReservedUnused(IntelBluetoothHostControllerUSBTransport, 11)
OSMetaClassDefineReservedUnused(IntelBluetoothHostControllerUSBTransport, 12)
OSMetaClassDefineReservedUnused(IntelBluetoothHostControllerUSBTransport, 13)
OSMetaClassDefineReservedUnused(IntelBluetoothHostControllerUSBTransport, 14)
OSMetaClassDefineReservedUnused(IntelBluetoothHostControllerUSBTransport, 15)
OSMetaClassDefineReservedUnused(IntelBluetoothHostControllerUSBTransport, 16)
OSMetaClassDefineReservedUnused(IntelBluetoothHostControllerUSBTransport, 17)
OSMetaClassDefineReservedUnused(IntelBluetoothHostControllerUSBTransport, 18)
OSMetaClassDefineReservedUnused(IntelBluetoothHostControllerUSBTransport, 19)
OSMetaClassDefineReservedUnused(IntelBluetoothHostControllerUSBTransport, 20)
OSMetaClassDefineReservedUnused(IntelBluetoothHostControllerUSBTransport, 21)
OSMetaClassDefineReservedUnused(IntelBluetoothHostControllerUSBTransport, 22)
OSMetaClassDefineReservedUnused(IntelBluetoothHostControllerUSBTransport, 23)
