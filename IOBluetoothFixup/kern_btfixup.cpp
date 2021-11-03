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

#include "kern_btfixup.hpp"

static IOBluetoothFixup * callback = nullptr;

void IOBluetoothFixup::init()
{
    callback = this;
    
    lilu.onKextLoadForce(kextList, arrsize(kextList),
    [](void *user, KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {
        callback->processKext(patcher, index, address, size);
    }, this);
}

void IOBluetoothFixup::deinit()
{
    
}

void IOBluetoothFixup::processKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size)
{
    if (index == kextList[0].loadIndex)
    {
        KernelPatcher::RouteRequest request (createBluetoothHostControllerObjectSymbol, CreateBluetoothHostControllerObject, orgIOBluetoothFamily_CreateBluetoothHostControllerObject);
        if (!patcher.routeMultiple(index, &request, 1, address, size))
        {
            SYSLOG("IOBluetoothFixup", "patcher.routeMultiple for %s failed with error %d", request.symbol, patcher.getError());
            patcher.clearError();
        }
    }
}

IOReturn IOBluetoothFixup::CreateBluetoothHostControllerObject(IOBluetoothHCIController * that, BluetoothHardwareListType * hardware)
{
    IOBluetoothHostController * controller;

    if ( !hardware || !hardware->mBluetoothTransport )
        return -536870212;
  
    switch ( *(UInt16 *) (hardware->mBluetoothTransport + 176) ) //mControllerVendorType
    {
        case 2:
        case 6:
        case 7:
            controller = OSTypeAlloc(AppleBroadcomBluetoothHostController);
            break;
        case 3:
            controller = OSTypeAlloc(AppleCSRBluetoothHostController);
            break;
        case 4:
            controller = OSTypeAlloc(BroadcomBluetoothHostController);
            break;
        case 5:
            controller = OSTypeAlloc(CSRBluetoothHostController);
            break;
        case 8:
            controller = OSTypeAlloc(IntelBluetoothHostController);
            break;
        default:
            controller = OSTypeAlloc(IOBluetoothHostController);
            break;
    }
    
    if ( !controller )
        return -536870212;
    
    if ( controller->init(that, hardware->mBluetoothTransport) && controller->attach(that) )
    {
        *(IOBluetoothHostControllerTransport **) (controller + 832) = hardware->mBluetoothTransport;
        if ( controller->start(that) )
        {
            *(IOBluetoothHostController **) (hardware->mBluetoothTransport + 144) = controller;
            hardware->mBluetoothHostController = controller;
            return 0;
        }
        controller->detach(that);
    }
    
    OSSafeReleaseNULL(controller);
    return -536870212;
}
