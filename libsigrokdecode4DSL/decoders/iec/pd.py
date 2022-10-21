##
## This file is part of the libsigrokdecode project.
##
## Copyright (C) 2017 Marcus Comstedt <marcus@mc.pp.se>
## Copyright (C) 2019 DreamSourceLab <support@dreamsourcelab.com>
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
## along with this program; if not, see <http://www.gnu.org/licenses/>.
##

import sigrokdecode as srd

step_wait_conds = (
    [{2: 'f'}, {0: 'l', 1: 'h'}],
    [{2: 'f'}, {0: 'h', 1: 'h'}, {1: 'l'}],
    [{2: 'f'}, {0: 'f'}, {1: 'l'}],
    [{2: 'f'}, {1: 'e'}],
)

class Decoder(srd.Decoder):
    api_version = 3
    id = 'iec'
    name = 'IEC'
    longname = 'Commodore IEC bus'
    desc = 'Commodore serial IEEE-488 (IEC) bus protocol.'
    license = 'gplv2+'
    inputs = ['logic']
    outputs = []
    tags = ['PC', 'Retro computing']
    channels = (
        {'id': 'data', 'name': 'DATA', 'desc': 'Data I/O', 'idn':'dec_iec_chan_data'},
        {'id': 'clk', 'name': 'CLK', 'desc': 'Clock', 'idn':'dec_iec_chan_clk'},
        {'id': 'atn', 'name': 'ATN', 'desc': 'Attention', 'idn':'dec_iec_chan_atn'},
    )
    optional_channels = (
        {'id': 'srq', 'name': 'SRQ', 'desc': 'Service request', 'idn':'dec_iec_opt_chan_srq'},
    )
    annotations = (
        ('items', 'Items'),
        ('gpib', 'DAT/CMD'),
        ('eoi', 'EOI'),
    )
    annotation_rows = (
        ('bytes', 'Bytes', (0,)),
        ('gpib', 'DAT/CMD', (1,)),
        ('eoi', 'EOI', (2,)),
    )

    def __init__(self):
        self.reset()

    def reset(self):
        self.saved_ATN = False
        self.saved_EOI = False
        self.ss_item = self.es_item = None
        self.step = 0
        self.bits = 0
        self.numbits = 0

    def start(self):
        self.out_ann = self.register(srd.OUTPUT_ANN)

    def putb(self, data):
        self.put(self.ss_item, self.es_item, self.out_ann, data)

    def handle_bits(self):
        # Output the saved item.
        dbyte = self.bits
        dATN = self.saved_ATN
        dEOI = self.saved_EOI
        self.es_item = self.samplenum
        self.putb([0, ['%02X' % dbyte]])

        # Encode item byte to GPIB convention.
        self.strgpib = ' '
        if dATN: # ATN, decode commands.
            # Note: Commands < 0x20 are not used on IEC bus.
            if dbyte == 0x01: self.strgpib = 'GTL'
            if dbyte == 0x04: self.strgpib = 'SDC'
            if dbyte == 0x05: self.strgpib = 'PPC'
            if dbyte == 0x08: self.strgpib = 'GET'
            if dbyte == 0x09: self.strgpib = 'TCT'
            if dbyte == 0x11: self.strgpib = 'LLO'
            if dbyte == 0x14: self.strgpib = 'DCL'
            if dbyte == 0x15: self.strgpib = 'PPU'
            if dbyte == 0x18: self.strgpib = 'SPE'
            if dbyte == 0x19: self.strgpib = 'SPD'

            if dbyte == 0x3f: self.strgpib = 'UNL'
            if dbyte == 0x5f: self.strgpib = 'UNT'
            if dbyte > 0x1f and dbyte < 0x3f: # Address listener.
                self.strgpib = 'L' + chr(dbyte + 0x10)
            if dbyte > 0x3f and dbyte < 0x5f: # Address talker.
                self.strgpib = 'T' + chr(dbyte - 0x10)
            if dbyte > 0x5f and dbyte < 0x70: # Channel reopen.
                self.strgpib = 'R' + chr(dbyte - 0x30)
            if dbyte > 0xdf and dbyte < 0xf0: # Channel close.
                self.strgpib = 'C' + chr(dbyte - 0xb0)
            if dbyte > 0xef: # Channel open.
                self.strgpib = 'O' + chr(dbyte - 0xc0)
        else:
            if dbyte > 0x1f and dbyte < 0x7f:
                self.strgpib = chr(dbyte)
            if dbyte == 0x0a:
                self.strgpib = 'LF'
            if dbyte == 0x0d:
                self.strgpib = 'CR'

        self.putb([1, [self.strgpib]])
        self.strEOI = ' '
        if dEOI:
            self.strEOI = 'EOI'
        self.putb([2, [self.strEOI]])

    def decode(self):
        while True:

            (data, clk, atn, srq) = self.wait(step_wait_conds[self.step])

            if (self.matched & (0b1 << 0)):
                # Falling edge on ATN, reset step.
                self.step = 0

            if self.step == 0:
                # Don't use self.matched_[1] here since we might come from
                # a step with different conds due to the code above.
                if data == 0 and clk == 1:
                    # Rising edge on CLK while DATA is low: Ready to send.
                    self.step = 1
            elif self.step == 1:
                if data == 1 and clk == 1:
                    # Rising edge on DATA while CLK is high: Ready for data.
                    self.ss_item = self.samplenum
                    self.saved_ATN = not atn
                    self.saved_EOI = False
                    self.bits = 0
                    self.numbits = 0
                    self.step = 2
                elif clk == 0:
                    # CLK low again, transfer aborted.
                    self.step = 0
            elif self.step == 2:
                if data == 0 and clk == 1:
                    # DATA goes low while CLK is still high, EOI confirmed.
                    self.saved_EOI = True
                elif clk == 0:
                    self.step = 3
            elif self.step == 3:
                if (self.matched & (0b1 << 1)):
                    if clk == 1:
                        # Rising edge on CLK; latch DATA.
                        self.bits |= data << self.numbits
                    elif clk == 0:
                        # Falling edge on CLK; end of bit.
                        self.numbits += 1
                        if self.numbits == 8:
                            self.handle_bits()
                            self.step = 0
