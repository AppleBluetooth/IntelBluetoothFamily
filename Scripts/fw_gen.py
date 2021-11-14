#!/usr/bin/python

#
# Released under "The GNU General Public License (GPL-2.0)"
#
# Copyright (c) 2021 williambj1. All rights reserved.
# Copyright (c) 2021 cjiang. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2 of the License, or (at your
# option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
# for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
#

import sys
import zlib
import os
import struct
import hashlib

copyright = '''/*
 *  Released under "The GNU General Public License (GPL-2.0)"
 *
 *  Copyright (c) 2021 cjiang. All rights reserved.
 *  Copyright (c) 2021 williambj1. All rights reserved.
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

#include <FirmwareList.h>
'''

def hash(data):
    sha1sum = hashlib.sha1()
    sha1sum.update(data)
    return sha1sum.hexdigest()

def format_var_name(hash):
    return "firmware_" + hash

def write_single_file(target_file, file_path, fw_root, file_hashes):
    src_file = open(file_path, "rb")
    src_data = src_file.read()
    src_hash = hash(src_data)
    data_var_name = format_var_name(src_hash)
    rel_path = os.path.relpath(file_path, fw_root).lstrip('.')

    for i in range(len(file_hashes)):
        if src_hash == file_hashes[i][1]:
            file_hash = (rel_path, src_hash, file_hashes[i][2])
            file_hashes.append(file_hash)
            return

    src_data = zlib.compress(src_data)
    src_len = len(src_data)
    file_hash = (rel_path, src_hash, src_len)
    file_hashes.append(file_hash)
    target_file.write("\nUInt8 ")
    target_file.write(data_var_name)
    target_file.write("[] = \n{\n")
    index = 0;
    block = []
    while True:
        if index + 16 >= src_len:
            block = src_data[index:]
        else:
            block = src_data[index:index + 16]
        index += 16;
        if len(block) < 16:
            if len(block):
                target_file.write("\t")
                for b in block:
                    if type(b) is str:
                        b = ord(b)
                    target_file.write("0x{:02X}, ".format(b))
                target_file.write("\n")
            break
        target_file.write("\t0x{:02X}, 0x{:02X}, 0x{:02X}, 0x{:02X}, 0x{:02X}, 0x{:02X}, 0x{:02X}, 0x{:02X}, 0x{:02X}, 0x{:02X}, 0x{:02X}, 0x{:02X}, 0x{:02X}, 0x{:02X}, 0x{:02X}, 0x{:02X},\n" .format(*struct.unpack("BBBBBBBBBBBBBBBB", block)))
    target_file.write("};\n")

    src_file.close()

def process_files(target_file, dir, extensions):
    if not os.path.exists(target_file):
        if not os.path.exists(os.path.dirname(target_file)):
            os.mkdir(os.path.dirname(target_file))
    target_file_handle = open(target_file, "w")
    target_file_handle.write(copyright)
    file_hashes = []
    for root, _, files in os.walk(dir):
        for file in files:
            path = os.path.join(root, file)
            if os.path.splitext(path)[1].lstrip('.') in extensions:
                write_single_file(target_file_handle, path, dir, file_hashes)

    target_file_handle.write("\n")
    target_file_handle.write("FirmwareDescriptor fwCandidates[] = \n{\n")

    for file_hash in file_hashes:
        target_file_handle.write('\t{ "')
        target_file_handle.write(file_hash[0])
        target_file_handle.write('", ')
        fw_var_name = format_var_name(file_hash[1])
        target_file_handle.write(fw_var_name)
        target_file_handle.write(", ")
        target_file_handle.write(str(file_hash[2]))
        target_file_handle.write(" },\n")

    target_file_handle.write("};\n\n")
    target_file_handle.write("int fwCount = ")
    target_file_handle.write(str(len(file_hashes)))
    target_file_handle.write(";")

    target_file_handle.close()

if __name__ == '__main__':
    process_files(sys.argv[1], sys.argv[2], sys.argv[3].split(','))
