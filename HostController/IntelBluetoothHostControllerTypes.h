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

#pragma once

#include <IOKit/bluetooth/Bluetooth.h>

enum BluetoothHCIIntelResetTypes
{
    kBluetoothHCIIntelResetTypeHardwareReset     = 0x00,
    kBluetoothHCIIntelResetTypeSoftWatchdogReset = 0x01,
    kBluetoothHCIIntelResetTypeSoftSoftwareReset = 0x02,
    kBluetoothHCIIntelResetTypeHardWatchdogReset = 0x03,
    kBluetoothHCIIntelResetTypeHardSoftwareReset = 0x04
};

enum BluetoothHCIIntelSoftwareResetTypes
{
    kBluetoothHCIIntelSoftwareResetTypeSoft = 0x00,
    kBluetoothHCIIntelSoftwareResetTypeHard = 0x01
};

enum BluetoothHCIIntelResetReaons
{
    kBluetoothHCIIntelResetReaonPowerOn           = 0x00,
    kBluetoothHCIIntelResetReaonResetCommand      = 0x01,
    kBluetoothHCIIntelResetReaonIntelResetCommand = 0x02,
    kBluetoothHCIIntelResetReaonWatchdog          = 0x03,
    kBluetoothHCIIntelResetReaonFatalException    = 0x04,
    kBluetoothHCIIntelResetReaonSystemException   = 0x05,
    kBluetoothHCIIntelResetReaonUnknown           = 0xFF
};

enum BluetoothHCIIntelResetDDCReloadModes
{
    kBluetoothHCIIntelResetDDCReloadModeDisabled = 0x00,
    kBluetoothHCIIntelResetDDCReloadModeOTP      = 0x01
};

enum kBluetoothHCIIntelSecureSendResults
{
    kBluetoothHCIIntelSecureSendResultSuccess = 0x00,
    kBluetoothHCIIntelSecureSendResultGeneralFailure,
    kBluetoothHCIIntelSecureSendResultHardwareFailure,
    kBluetoothHCIIntelSecureSendResultSignatureVerificationFailure,
    kBluetoothHCIIntelSecureSendResultCommandBufferParseError,
    kBluetoothHCIIntelSecureSendResultCommandExecutionFailure,
    kBluetoothHCIIntelSecureSendResultCommandParametersError,
    kBluetoothHCIIntelSecureSendResultCommandMissing
};

enum kBluetoothHCIIntelLinkPDUTraceTypes
{
    kBluetoothHCIIntelLinkPDUTraceTypeLMPRx  = 0x00,
    kBluetoothHCIIntelLinkPDUTraceTypeLMPTx  = 0x01,
    kBluetoothHCIIntelLinkPDUTraceTypeLMPAck = 0x02,
    
    kBluetoothHCIIntelLinkPDUTraceTypeLLRx   = 0x03,
    kBluetoothHCIIntelLinkPDUTraceTypeLLTx   = 0x04,
    kBluetoothHCIIntelLinkPDUTraceTypeLLAck  = 0x05
};

enum BluetoothHCIIntelDDCStatus
{
    kBluetoothHCIIntelDDCStatusFirmwareDefault        = 0x00,
    kBluetoothHCIIntelDDCStatusFirmwareDefaultWithOTP = 0x01,
    kBluetoothHCIIntelDDCStatusPersistentRAM          = 0x02,
    kBluetoothHCIIntelDDCStatusReserved
};

enum BluetoothHCIIntelExceptionTypes
{
    kBluetoothHCIIntelExceptionTypeNoException          = 0x00,
    kBluetoothHCIIntelExceptionTypeUndefinedInstruction = 0x01,
    kBluetoothHCIIntelExceptionTypePrefetchAbort        = 0x02,
    kBluetoothHCIIntelExceptionTypeDataAbort            = 0x03,
};

enum BluetoothHCIIntelStimulatedExceptionTypes
{
    kBluetoothHCIIntelStimulatedExceptionTypeFatalException = 0x01,
    kBluetoothHCIIntelStimulatedExceptionTypeDebugException = 0x02
};

enum BluetoothHCIIntelMemoryModes
{
    kBluetoothHCIIntelMemoryModeByteAccess     = 0x00,
    kBluetoothHCIIntelMemoryModeHalfWordAccess = 0x01,
    kBluetoothHCIIntelMemoryModeWordAccess     = 0x02,
};

typedef enum BluetoothIntelHardwareVariant
{
    kBluetoothIntelHardwareVariantWP     = 0x07,    // Legacy ROM
    kBluetoothIntelHardwareVariantStP    = 0x08,    // Legacy ROM
    kBluetoothIntelHardwareVariantSfP    = 0x0b,
    kBluetoothIntelHardwareVariantWsP    = 0x0c,
    kBluetoothIntelHardwareVariantJfP    = 0x11,
    kBluetoothIntelHardwareVariantThP    = 0x12,
    kBluetoothIntelHardwareVariantHrP    = 0x13,
    kBluetoothIntelHardwareVariantCcP    = 0x14,
    kBluetoothIntelHardwareVariantTyP    = 0x17,
    kBluetoothIntelHardwareVariantSlr    = 0x18,
    kBluetoothIntelHardwareVariantSlrF   = 0x19
} BluetoothIntelHardwareVariant;

/*! @enum        IntelExitManufacturerModeResetOptions
     @abstract    Options for the second command parameter in the manufacturing exit HCI command
     @discussion  In Intel's vendor specific manufacturing exit HCI command, the second parameter denotes what shall be done besides the regular disabling, such as a reset.
     @constant    kIntelExitManufacturerModeResetOptionsNoReset                                                No extra operations
     @constant    kBluetoothIntelManufacturingExitResetOptionResetDeactivatePatches                Reset without patches
     @constant    kBluetoothIntelManufacturingExitResetOptionResetActivatePatches                    Reset with patches
*/

typedef UInt8 BluetoothIntelManufacturingExitResetOption;
enum BluetoothIntelManufacturingExitResetOptions
{
    kBluetoothIntelManufacturingExitResetOptionsNoReset,
    kBluetoothIntelManufacturingExitResetOptionResetDeactivatePatches,
    kBluetoothIntelManufacturingExitResetOptionResetActivatePatches
};

enum BluetoothIntelTLVTypes
{
    kBluetoothIntelTLVTypeCNVITop = 0x10,
    kBluetoothIntelTLVTypeCNVRTop,
    kBluetoothIntelTLVTypeCNVIBT,
    kBluetoothIntelTLVTypeCNVRBT,
    kBluetoothIntelTLVTypeCNVIOTP,
    kBluetoothIntelTLVTypeCNVROTP,
    kBluetoothIntelTLVTypeDeviceRevisionID,
    kBluetoothIntelTLVTypeUSBVendorID,
    kBluetoothIntelTLVTypeUSBProductID,
    kBluetoothIntelTLVTypePCIeVendorID,
    kBluetoothIntelTLVTypePCIeDeviceID,
    kBluetoothIntelTLVTypePCIeSubsystemID,
    kBluetoothIntelTLVTypeImageType,
    kBluetoothIntelTLVTypeTimestamp,
    kBluetoothIntelTLVTypeBuildType,
    kBluetoothIntelTLVTypeBuildNum,
    kBluetoothIntelTLVTypeFirmwareBuildProduct,
    kBluetoothIntelTLVTypeFirmwareBuildHardware,
    kBluetoothIntelTLVTypeFirmwareStep,
    kBluetoothIntelTLVTypeBluetoothSpecification,
    kBluetoothIntelTLVTypeManufacturingName,
    kBluetoothIntelTLVTypeHCIRevision,
    kBluetoothIntelTLVTypeLMPSubversion,
    kBluetoothIntelTLVTypeOTPPatchVersion,
    kBluetoothIntelTLVTypeSecureBoot,
    kBluetoothIntelTLVTypeKeyFromHeader,
    kBluetoothIntelTLVTypeOTPLock,
    kBluetoothIntelTLVTypeAPILock,
    kBluetoothIntelTLVTypeDebugLock,
    kBluetoothIntelTLVTypeMinimumFirmware,
    kBluetoothIntelTLVTypeLimitedCCE,
    kBluetoothIntelTLVTypeSBEType,
    kBluetoothIntelTLVTypeOTPDeviceAddress,
    kBluetoothIntelTLVTypeUnlockedState
};

enum BluetoothHCIIntelFirmwareVariants
{
    kBluetoothHCIIntelFirmwareVariantLegacyROM2_5 = 0x01,
    kBluetoothHCIIntelFirmwareVariantBootloader   = 0x06,
    kBluetoothHCIIntelFirmwareVariantLegacyROM2_X = 0x22,
    kBluetoothHCIIntelFirmwareVariantFirmware     = 0x23
};
typedef UInt8 BluetoothHCIIntelFirmwareVariant;

enum BluetoothHCIIntelImageTypes
{
    kBluetoothHCIIntelImageTypeBootloader = 0x01,
    kBluetoothHCIIntelImageTypeFirmware   = 0x03
};
typedef UInt8 BluetoothHCIIntelImageType;

typedef enum BluetoothHCIIntelSecureSendFragmentType
{
    kBluetoothHCIIntelFirmwareFragmentTypeInit = 0x00,
    kBluetoothHCIIntelFirmwareFragmentTypeData = 0x01,
    kBluetoothHCIIntelFirmwareFragmentTypeSign = 0x02,
    kBluetoothHCIIntelFirmwareFragmentTypePKey = 0x03
} BluetoothHCIIntelSecureSendFragmentType;

enum BluetoothHCIIntelEvents
{
    kBluetoothHCIEventIntelFatalException           = 0x01,
    kBluetoothHCIEventIntelBootup                   = 0x02,
    kBluetoothHCIEventIntelDefaultBDData            = 0x05,
    kBluetoothHCIEventIntelSecureSendCommandsResult = 0x06,
    kBluetoothHCIEventIntelDebugException           = 0x08,
    kBluetoothHCIEventIntelLELinkEstablished        = 0x0F,
    kBluetoothHCIEventIntelScanStatus               = 0x11,
    kBluetoothHCIEventIntelTraceActivationComplete  = 0x16,
    kBluetoothHCIEventIntelLinkPDUTrace             = 0x17,
    kBluetoothHCIEventIntelWriteBDDataComplete      = 0x19,
    kBluetoothHCIEventIntelSCORejectedViaLMP        = 0x25,
    kBluetoothHCIEventIntelPTTSwitchNotification    = 0x26,
    kBluetoothHCIEventIntelSystemException          = 0x29,
    kBluetoothHCIEventIntelFirmwareTraceString      = 0x2C,
    kBluetoothHCIEventIntelFirmwareTraceBinary      = 0x2E
};

enum BluetoothHCIIntelCommands
{
    kBluetoothHCIIntelCommandReset                = 0x0001,
    kBluetoothHCIIntelCommandNoOperation          = 0x0002,
    kBluetoothHCIIntelCommandReadVersionInfo      = 0x0005,
    kBluetoothHCIIntelCommandSetUARTBaudrate      = 0x0006,
    kBluetoothHCIIntelCommandEnableLPM            = 0x0007,
    kBluetoothHCIIntelCommandPCMWriteConfig       = 0x0008,
    kBluetoothHCIIntelCommandSecureSend           = 0x0009,
    kBluetoothHCIIntelCommandReadBootParams       = 0x000D,
    kBluetoothHCIIntelCommandWriteBootParams      = 0x000E,
    kBluetoothHCIIntelCommandUnlock               = 0x000F,
    kBluetoothHCIIntelCommandChangeUARTBaudrate   = 0x0010,
    kBluetoothHCIIntelCommandManufacturerMode     = 0x0011,
    kBluetoothHCIIntelCommandReadLinkRSSI         = 0x0012,
    kBluetoothHCIIntelCommandMicrosoftExtension   = 0x001E,
    kBluetoothHCIIntelCommandReadExceptionInfo    = 0x0022,
    kBluetoothHCIIntelCommandClearExceptionInfo   = 0x0024,
    kBluetoothHCIIntelCommandWriteBDData          = 0x002F,
    kBluetoothHCIIntelCommandReadBDData           = 0x0030,
    kBluetoothHCIIntelCommandWriteDeviceAddress   = 0x0031,
    kBluetoothHCIIntelCommandFlowSpecification    = 0x0032,
    kBluetoothHCIIntelCommandReadSecureID         = 0x0034,
    kBluetoothHCIIntelCommandSetSyncInterfaceType = 0x0038,
    kBluetoothHCIIntelCommandConfigSyncInterface  = 0x0039,
    kBluetoothHCIIntelCommandSWRFKill             = 0x003F,
    kBluetoothHCIIntelCommandActivateTraces       = 0x0043, // also handles deactivation
    kBluetoothHCIIntelCommandStimulateException   = 0x004D,
    kBluetoothHCIIntelCommandReadHardwareVersion  = 0x0050,
    kBluetoothHCIIntelCommandSetEventMask         = 0x0052,
    kBluetoothHCIIntelCommandConfigLinkController = 0x0053,
    kBluetoothHCIIntelCommandReadOffloadUseCases  = 0x0086,
    kBluetoothHCIIntelCommandWriteDDC             = 0x0089,
    kBluetoothHCIIntelCommandReadDDC              = 0x008A,
    kBluetoothHCIIntelCommandWriteConfigDDC       = 0x008B,
    kBluetoothHCIIntelCommandReadConfigDDC        = 0x008C,
    kBluetoothHCIIntelCommandReadMemory           = 0x008D,
    kBluetoothHCIIntelCommandWriteMemory          = 0x008E,
    kBluetoothHCIIntelCommandSetLinkStatsTracing  = 0x00A1,
    kBluetoothHCIIntelCommandReadDebugFeatures    = 0x00A6
};

struct BluetoothIntelVersionInfoTLV
{
    UInt32 cnviTop;
    UInt32 cnvrTop;
    UInt32 cnviBT;
    UInt32 cnvrBT;
    UInt16 deviceRevisionID;
    UInt8  imageType;
    UInt16 timestamp;
    UInt8  buildType;
    UInt32 buildNumber;
    UInt8  secureBoot;
    UInt8  otpLock;
    UInt8  apiLock;
    UInt8  debugLock;
    UInt8  firmwareBuildNumber;
    UInt8  firmwareBuildWeek;
    UInt8  firmwareBuildYear;
    UInt8  limitedCCE;
    UInt8  sbeType;
    BluetoothDeviceAddress otpDeviceAddress;
};

struct BluetoothIntelTLV
{
    UInt8  type;
    UInt8  length;
    UInt8  value[];
} __attribute__((packed));

struct BluetoothIntelVersionInfo
{
    UInt8  hardwarePlatform;
    UInt8  hardwareVariant;
    UInt8  hardwareRevision;
    UInt8  firmwareVariant;
    UInt8  firmwareRevision;
    UInt8  firmwareBuildNum;
    UInt8  firmwareBuildWeek;
    UInt8  firmwareBuildYear;
    UInt8  firmwarePatchVersion;
} __attribute__((packed));

struct BluetoothIntelBootParams
{
    UInt8  otpFormat;
    UInt8  otpContent;
    UInt8  otpPatch;
    SInt16 deviceRevisionID;
    UInt8  secureBoot;
    UInt8  keyFromHeader;
    UInt8  keyType;
    UInt8  otpLock;
    UInt8  apiLock;
    UInt8  debugLock;
    BluetoothDeviceAddress otpDeviceAddress;
    UInt8  minFirmwareBuildNumber;
    UInt8  minFirmwareBuildWeek;
    UInt8  minFirmwareBuildYear;
    UInt8  limitedCCE;
    UInt8  unlockedState;
} __attribute__((packed));

struct BluetoothIntelDebugFeatures
{
    UInt8 unknown1;
    UInt8 unknown2;
    UInt8 page1[16];
} __attribute__((packed));

struct BluetoothIntelCommandWriteBootParams
{
    UInt32 bootAddress;
    UInt8  firmwareBuildNumber;
    UInt8  firmwareBuildWeek;
    UInt8  firmwareBuildYear;
} __attribute__((packed));

struct BluetoothIntelBootupEventParams
{
    UInt8  zero;
    UInt8  numCommands;
    UInt8  source;
    UInt8  resetType;
    UInt8  resetReason;
    UInt8  ddcStatus;
} __attribute__((packed));

struct BluetoothIntelSecureSendResultEventParams
{
    UInt8  result;
    UInt16 opCode;
    UInt8  status;
} __attribute__((packed));

struct BluetoothIntelOffloadUseCases
{
    UInt8 preset[8];
} __attribute__((packed));

struct BluetoothIntelExceptionInfo
{
    char info[12];
} __attribute__((packed));

#define IntelCNVXExtractHardwarePlatform(cnvx)      ((UInt8)(((cnvx) & 0x0000ff00) >> 8))
#define IntelCNVXExtractHardwareVariant(cnvx)       ((UInt8)(((cnvx) & 0x003f0000) >> 16))
#define IntelCNVXTopExtractType(cnvxTop)            ((cnvxTop) & 0x00000fff)
#define IntelCNVXTopExtractStep(cnvxTop)            (((cnvxTop) & 0x0f000000) >> 24)
#define IntelMakeCNVXTopEndianSwap(type, step)      OSSwapInt16(((UInt16)(((type) << 4) | (step))))

#define kIntelRSAHeaderLength      644
#define kIntelECDSAHeaderLength    320
#define kIntelCSSHeaderOffset      8
#define kIntelECDSAOffset          644
