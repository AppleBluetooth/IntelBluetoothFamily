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

#ifndef Logs_h
#define Logs_h

#define RequireFailureLog(err) os_log(OS_LOG_DEFAULT, "REQUIRE failure: %s - file: %s:%d\n", err, __FILE__, __LINE__)
#define CheckFailureLog(err) os_log(OS_LOG_DEFAULT, "CHECK failure: %s - file: %s:%d\n", err, __FILE__, __LINE__)
#define REQUIRE_NO_ERR(err) os_log(OS_LOG_DEFAULT, "REQUIRE_NO_ERR failure: 0x%x - file: %s:%d\n", err, __FILE__, __LINE__)

#endif /* Logs_h */
