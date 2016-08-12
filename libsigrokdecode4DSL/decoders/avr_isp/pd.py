##
## This file is part of the libsigrokdecode project.
##
## Copyright (C) 2012-2014 Uwe Hermann <uwe@hermann-uwe.de>
##
## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 2 of the License, or
## (at your option) any later version.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program; if not, write to the Free Software
## Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
##

import sigrokdecode as srd
from .parts import *

VENDOR_CODE_ATMEL = 0x1e

class Decoder(srd.Decoder):
    api_version = 2
    id = 'avr_isp'
    name = 'AVR ISP'
    longname = 'AVR In-System Programming'
    desc = 'Protocol for in-system programming Atmel AVR MCUs.'
    license = 'gplv2+'
    inputs = ['spi']
    outputs = ['avr_isp']
    annotations = (
        ('pe', 'Programming enable'),
        ('rsb0', 'Read signature byte 0'),
        ('rsb1', 'Read signature byte 1'),
        ('rsb2', 'Read signature byte 2'),
        ('ce', 'Chip erase'),
        ('rfb', 'Read fuse bits'),
        ('rhfb', 'Read high fuse bits'),
        ('refb', 'Read extended fuse bits'),
        ('warnings', 'Warnings'),
        ('dev', 'Device'),
    )
    annotation_rows = (
        ('bits', 'Bits', ()),
        ('commands', 'Commands', tuple(range(7 + 1))),
        ('warnings', 'Warnings', (8,)),
        ('dev', 'Device', (9,)),
    )

    def __init__(self):
        self.state = 'IDLE'
        self.mosi_bytes, self.miso_bytes = [], []
        self.ss_cmd, self.es_cmd = 0, 0
        self.xx, self.yy, self.zz, self.mm = 0, 0, 0, 0
        self.ss_device = None

    def start(self):
        self.out_ann = self.register(srd.OUTPUT_ANN)

    def putx(self, data):
        self.put(self.ss_cmd, self.es_cmd, self.out_ann, data)

    def handle_cmd_programming_enable(self, cmd, ret):
        # Programming enable.
        # Note: The chip doesn't send any ACK for 'Programming enable'.
        self.putx([0, ['Programming enable']])

        # Sanity check on reply.
        if ret[1:4] != [0xac, 0x53, cmd[2]]:
            self.putx([8, ['Warning: Unexpected bytes in reply!']])

    def handle_cmd_read_signature_byte_0x00(self, cmd, ret):
        # Signature byte 0x00: vendor code.
        self.vendor_code = ret[3]
        v = vendor_code[self.vendor_code]
        self.putx([1, ['Vendor code: 0x%02x (%s)' % (ret[3], v)]])

        # Store for later.
        self.xx = cmd[1] # Same as ret[2].
        self.yy = cmd[3]
        self.zz = ret[0]

        # Sanity check on reply.
        if ret[1] != 0x30 or ret[2] != cmd[1]:
            self.putx([8, ['Warning: Unexpected bytes in reply!']])

        # Sanity check for the vendor code.
        if self.vendor_code != VENDOR_CODE_ATMEL:
            self.putx([8, ['Warning: Vendor code was not 0x1e (Atmel)!']])

    def handle_cmd_read_signature_byte_0x01(self, cmd, ret):
        # Signature byte 0x01: part family and memory size.
        self.part_fam_flash_size = ret[3]
        self.putx([2, ['Part family / memory size: 0x%02x' % ret[3]]])

        # Store for later.
        self.mm = cmd[3]
        self.ss_device = self.ss_cmd

        # Sanity check on reply.
        if ret[1] != 0x30 or ret[2] != cmd[1] or ret[0] != self.yy:
            self.putx([8, ['Warning: Unexpected bytes in reply!']])

    def handle_cmd_read_signature_byte_0x02(self, cmd, ret):
        # Signature byte 0x02: part number.
        self.part_number = ret[3]
        self.putx([3, ['Part number: 0x%02x' % ret[3]]])

        p = part[(self.part_fam_flash_size, self.part_number)]
        data = [9, ['Device: Atmel %s' % p]]
        self.put(self.ss_device, self.es_cmd, self.out_ann, data)

        # Sanity check on reply.
        if ret[1] != 0x30 or ret[2] != self.xx or ret[0] != self.mm:
            self.putx([8, ['Warning: Unexpected bytes in reply!']])

        self.xx, self.yy, self.zz, self.mm = 0, 0, 0, 0

    def handle_cmd_chip_erase(self, cmd, ret):
        # Chip erase (erases both flash an EEPROM).
        # Upon successful chip erase, the lock bits will also be erased.
        # The only way to end a Chip Erase cycle is to release RESET#.
        self.putx([4, ['Chip erase']])

        # TODO: Check/handle RESET#.

        # Sanity check on reply.
        bit = (ret[2] & (1 << 7)) >> 7
        if ret[1] != 0xac or bit != 1 or ret[3] != cmd[2]:
            self.putx([8, ['Warning: Unexpected bytes in reply!']])

    def handle_cmd_read_fuse_bits(self, cmd, ret):
        # Read fuse bits.
        self.putx([5, ['Read fuse bits: 0x%02x' % ret[3]]])

        # TODO: Decode fuse bits.
        # TODO: Sanity check on reply.

    def handle_cmd_read_fuse_high_bits(self, cmd, ret):
        # Read fuse high bits.
        self.putx([6, ['Read fuse high bits: 0x%02x' % ret[3]]])

        # TODO: Decode fuse bits.
        # TODO: Sanity check on reply.

    def handle_cmd_read_extended_fuse_bits(self, cmd, ret):
        # Read extended fuse bits.
        self.putx([7, ['Read extended fuse bits: 0x%02x' % ret[3]]])

        # TODO: Decode fuse bits.
        # TODO: Sanity check on reply.

    def handle_command(self, cmd, ret):
        if cmd[:2] == [0xac, 0x53]:
            self.handle_cmd_programming_enable(cmd, ret)
        elif cmd[0] == 0xac and (cmd[1] & (1 << 7)) == (1 << 7):
            self.handle_cmd_chip_erase(cmd, ret)
        elif cmd[:3] == [0x50, 0x00, 0x00]:
            self.handle_cmd_read_fuse_bits(cmd, ret)
        elif cmd[:3] == [0x58, 0x08, 0x00]:
            self.handle_cmd_read_fuse_high_bits(cmd, ret)
        elif cmd[:3] == [0x50, 0x08, 0x00]:
            self.handle_cmd_read_extended_fuse_bits(cmd, ret)
        elif cmd[0] == 0x30 and cmd[2] == 0x00:
            self.handle_cmd_read_signature_byte_0x00(cmd, ret)
        elif cmd[0] == 0x30 and cmd[2] == 0x01:
            self.handle_cmd_read_signature_byte_0x01(cmd, ret)
        elif cmd[0] == 0x30 and cmd[2] == 0x02:
            self.handle_cmd_read_signature_byte_0x02(cmd, ret)
        else:
            c = '%02x %02x %02x %02x' % tuple(cmd)
            r = '%02x %02x %02x %02x' % tuple(ret)
            self.putx([0, ['Unknown command: %s (reply: %s)!' % (c, r)]])

    def decode(self, ss, es, data):
        ptype, mosi, miso = data

        # For now, only use DATA and BITS packets.
        if ptype not in ('DATA', 'BITS'):
            return

        # Store the individual bit values and ss/es numbers. The next packet
        # is guaranteed to be a 'DATA' packet belonging to this 'BITS' one.
        if ptype == 'BITS':
            self.miso_bits, self.mosi_bits = miso, mosi
            return

        self.ss, self.es = ss, es

        if len(self.mosi_bytes) == 0:
            self.ss_cmd = ss

        # Append new bytes.
        self.mosi_bytes.append(mosi)
        self.miso_bytes.append(miso)

        # All commands consist of 4 bytes.
        if len(self.mosi_bytes) < 4:
            return

        self.es_cmd = es

        self.handle_command(self.mosi_bytes, self.miso_bytes)

        self.mosi_bytes = []
        self.miso_bytes = []
