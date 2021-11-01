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

#ifndef AppleBluetoothHostControllers_h
#define AppleBluetoothHostControllers_h

#include <IOKit/bluetooth/IOBluetoothHostController.h>

/*
 * These classes do not represent their actaul declaration by Apple in IOBluetoothFamily. Only their structors are
 * defined so that they could be allocated by this kext while being kept simple.
 */

class BroadcomBluetoothHostController : public IOBluetoothHostController
{
    OSDeclareAbstractStructors(BroadcomBluetoothHostController)
};

class AppleBroadcomBluetoothHostController : public IOBluetoothHostController
{
    OSDeclareDefaultStructors(AppleBroadcomBluetoothHostController)
};

class CSRBluetoothHostController : public IOBluetoothHostController
{
    OSDeclareAbstractStructors(CSRBluetoothHostController)
};

class AppleCSRBluetoothHostController : public IOBluetoothHostController
{
    OSDeclareDefaultStructors(AppleCSRBluetoothHostController)
};

#endif /* AppleBluetoothHostControllers_h */
