/*
 *  Released under "The GNU General Public License (GPL-2.0)"
 *
 *  Copyright (c) 2021 cjiang. All rights reserved.
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

#ifndef kern_btfixup_hpp
#define kern_btfixup_hpp

#include <Headers/kern_patcher.hpp>
#include <Headers/kern_api.hpp>

#include <IOKit/bluetooth/IOBluetoothHCIController.h>
#include <IOKit/bluetooth/transport/IOBluetoothHostControllerTransport.h>

#include "BluetoothHostControllers.h"

static const char *kextIOBluetoothFamily[] { "/System/Library/Extensions/IOBluetoothFamily.kext/Contents/MacOS/IOBluetoothFamily" };
static KernelPatcher::KextInfo kextList[] {
    {"com.apple.iokit.IOBluetoothFamily", kextIOBluetoothFamily, arrsize(kextIOBluetoothFamily), {true}, {}, KernelPatcher::KextInfo::Unloaded }
};

static const char * createBluetoothHostControllerObjectSymbol { "__ZN24IOBluetoothHCIController35CreateBluetoothHostControllerObjectEP25BluetoothHardwareListType" }; //10.15-11.6

class IOBluetoothFixup
{
public:
    void init();
    void deinit();
    
private:
    void processKext(KernelPatcher &patcher, size_t index, mach_vm_address_t addres, size_t size);
    static IOReturn CreateBluetoothHostControllerObject(IOBluetoothHCIController * that, BluetoothHardwareListType * hardware);
    
    mach_vm_address_t orgIOBluetoothFamily_CreateBluetoothHostControllerObject {0};
};

#endif /* kern_btfixup_hpp */
