##
## This file is part of the libsigrokdecode project.
##
## Copyright (C) 2015 Paul Evans <leonerd@leonerd.org.uk>
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

import re
import sigrokdecode as srd

def _decode_intensity(val):
    intensity = val & 0x0f
    if intensity == 0:
        return 'min'
    elif intensity == 15:
        return 'max'
    else:
        return intensity

registers = {
    0x00: ['No-op', lambda _: ''],
    0x09: ['Decode', lambda v: '0b{:08b}'.format(v)],
    0x0A: ['Intensity', _decode_intensity],
    0x0B: ['Scan limit', lambda v: 1 + v],
    0x0C: ['Shutdown', lambda v: 'off' if v else 'on'],
    0x0F: ['Display test', lambda v: 'on' if v else 'off']
}

ann_reg, ann_digit, ann_warning = range(3)

class Decoder(srd.Decoder):
    api_version = 2
    id = 'max7219'
    name = 'MAX7219'
    longname = 'Maxim MAX7219/MAX7221'
    desc = '8-digit LED display driver.'
    license = 'gplv2+'
    inputs = ['spi']
    outputs = ['max7219']
    annotations = (
        ('register', 'Registers written to the device'),
        ('digit', 'Digits displayed on the device'),
        ('warnings', 'Human-readable warnings'),
    )
    annotation_rows = (
        ('commands', 'Commands', (ann_reg, ann_digit)),
        ('warnings', 'Warnings', (ann_warning,)),
    )

    def start(self):
        self.out_ann = self.register(srd.OUTPUT_ANN)
        self.pos = 0
        self.cs_start = 0

    def putreg(self, ss, es, reg, value):
        self.put(ss, es, self.out_ann, [ann_reg, ['%s: %s' % (reg, value)]])

    def putdigit(self, ss, es, digit, value):
        self.put(ss, es, self.out_ann, [ann_digit, ['Digit %d: %02X' % (digit, value)]])

    def putwarn(self, ss, es, message):
        self.put(ss, es, self.out_ann, [ann_warning, [message]])

    def decode(self, ss, es, data):
        ptype, mosi, _ = data

        if ptype == 'DATA':
            if not self.cs_asserted:
                return

            if self.pos == 0:
                self.addr = mosi
                self.addr_start = ss
            elif self.pos == 1:
                if self.addr >= 1 and self.addr <= 8:
                    self.putdigit(self.addr_start, es, self.addr, mosi)
                elif self.addr in registers:
                    name, decoder = registers[self.addr]
                    self.putreg(self.addr_start, es, name, decoder(mosi))
                else:
                    self.putwarn(self.addr_start, es,
                        'Unknown register %02X' % (self.addr))

            self.pos += 1
        elif ptype == 'CS-CHANGE':
            self.cs_asserted = mosi
            if self.cs_asserted:
                self.pos = 0
                self.cs_start = ss
            else:
                if self.pos == 1:
                    # Don't warn if pos=0 so that CS# glitches don't appear
                    # as spurious warnings.
                    self.putwarn(self.cs_start, es, 'Short write')
                elif self.pos > 2:
                    self.putwarn(self.cs_start, es, 'Overlong write')
