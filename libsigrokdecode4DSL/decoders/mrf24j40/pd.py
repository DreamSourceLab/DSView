##
## This file is part of the libsigrokdecode project.
##
## Copyright (C) 2015 Karl Palsson <karlp@tweak.net.au>
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
from .lists import *

class Decoder(srd.Decoder):
    api_version = 2
    id = 'mrf24j40'
    name = 'MRF24J40'
    longname = 'Microchip MRF24J40'
    desc = 'IEEE 802.15.4 2.4 GHz RF tranceiver chip.'
    license = 'gplv2+'
    inputs = ['spi']
    outputs = ['mrf24j40']
    annotations = (
        ('sread', 'Short register read commands'),
        ('swrite', 'Short register write commands'),
        ('lread', 'Long register read commands'),
        ('lwrite', 'Long register write commands'),
        ('warning', 'Warnings'),
    )
    annotation_rows = (
        ('read', 'Read', (0, 2)),
        ('write', 'Write', (1, 3)),
        ('warnings', 'Warnings', (4,)),
    )

    def __init__(self):
        self.ss_cmd, self.es_cmd = 0, 0
        self.mosi_bytes = []
        self.miso_bytes = []

    def start(self):
        self.out_ann = self.register(srd.OUTPUT_ANN)

    def putx(self, data):
        self.put(self.ss_cmd, self.es_cmd, self.out_ann, data)

    def putw(self, pos, msg):
        self.put(pos[0], pos[1], self.out_ann, [4, [msg]])

    def reset(self):
        self.mosi_bytes = []
        self.miso_bytes = []

    def handle_short(self):
        write = self.mosi_bytes[0] & 0x1
        reg = (self.mosi_bytes[0] >> 1) & 0x3f
        reg_desc = sregs.get(reg, 'illegal')
        if write:
            self.putx([1, ['%s: %#x' % (reg_desc, self.mosi_bytes[1])]])
        else:
            self.putx([0, ['%s: %#x' % (reg_desc, self.miso_bytes[1])]])

    def handle_long(self):
        dword = self.mosi_bytes[0] << 8 | self.mosi_bytes[1]
        write = dword & (0x1 << 4)
        reg = dword >> 5 & 0x3ff
        if reg >= 0x0:
            reg_desc = 'TX:%#x' % reg
        if reg >= 0x80:
            reg_desc = 'TX beacon:%#x' % reg
        if reg >= 0x100:
            reg_desc = 'TX GTS1:%#x' % reg
        if reg >= 0x180:
            reg_desc = 'TX GTS2:%#x' % reg
        if reg >= 0x200:
            reg_desc = lregs.get(reg, 'illegal')
        if reg >= 0x280:
            reg_desc = 'Security keys:%#x' % reg
        if reg >= 0x2c0:
            reg_desc = 'Reserved:%#x' % reg
        if reg >= 0x300:
            reg_desc = 'RX:%#x' % reg

        if write:
            self.putx([3, ['%s: %#x' % (reg_desc, self.mosi_bytes[2])]])
        else:
            self.putx([2, ['%s: %#x' % (reg_desc, self.miso_bytes[2])]])

    def decode(self, ss, es, data):
        ptype = data[0]
        if ptype == 'CS-CHANGE':
            # If we transition high mid-stream, toss out our data and restart.
            cs_old, cs_new = data[1:]
            if cs_old is not None and cs_old == 0 and cs_new == 1:
                if len(self.mosi_bytes) not in (0, 2, 3):
                    self.putw([self.ss_cmd, es], 'Misplaced CS!')
                    self.reset()
            return

        # Don't care about anything else.
        if ptype != 'DATA':
            return
        mosi, miso = data[1:]

        self.ss, self.es = ss, es

        if len(self.mosi_bytes) == 0:
            self.ss_cmd = ss
        self.mosi_bytes.append(mosi)
        self.miso_bytes.append(miso)

        # Everything is either 2 bytes or 3 bytes.
        if len(self.mosi_bytes) < 2:
            return

        if self.mosi_bytes[0] & 0x80:
            if len(self.mosi_bytes) == 3:
                self.es_cmd = es
                self.handle_long()
                self.reset()
        else:
            self.es_cmd = es
            self.handle_short()
            self.reset()
