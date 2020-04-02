##
## This file is part of the libsigrokdecode project.
##
## Copyright (C) 2016 Fabian J. Stumpf <sigrok@fabianstumpf.de>
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

class Decoder(srd.Decoder):
    api_version = 3
    id = 'dmx512'
    name = 'DMX512'
    longname = 'Digital MultipleX 512'
    desc = 'Digital MultipleX 512 (DMX512) lighting protocol.'
    license = 'gplv2+'
    inputs = ['logic']
    outputs = []
    tags = ['Embedded/industrial', 'Lighting']
    channels = (
        {'id': 'dmx', 'name': 'DMX data', 'desc': 'Any DMX data line'},
    )
    options = (
        {'id': 'invert', 'desc': 'Invert Signal?', 'default': 'no',
            'values': ('yes', 'no')},
    )
    annotations = (
        ('bit', 'Bit'),
        ('break', 'Break'),
        ('mab', 'Mark after break'),
        ('startbit', 'Start bit'),
        ('stopbits', 'Stop bit'),
        ('startcode', 'Start code'),
        ('channel', 'Channel'),
        ('interframe', 'Interframe'),
        ('interpacket', 'Interpacket'),
        ('data', 'Data'),
        ('error', 'Error'),
    )
    annotation_rows = (
        ('name', 'Logical', (1, 2, 5, 6, 7, 8)),
        ('data', 'Data', (9,)),
        ('bits', 'Bits', (0, 3, 4)),
        ('errors', 'Errors', (10,)),
    )

    def __init__(self):
        self.reset()

    def reset(self):
        self.samplerate = None
        self.sample_usec = None
        self.run_start = -1
        self.state = 'FIND BREAK'

    def start(self):
        self.out_ann = self.register(srd.OUTPUT_ANN)

    def metadata(self, key, value):
        if key == srd.SRD_CONF_SAMPLERATE:
            self.samplerate = value
            self.sample_usec = 1 / value * 1000000
            self.skip_per_bit = int(4 / self.sample_usec)

    def putr(self, data):
        self.put(self.run_start, self.samplenum, self.out_ann, data)

    def decode(self):
        if not self.samplerate:
            raise SamplerateError('Cannot decode without samplerate.')

        inv = self.options['invert'] == 'yes'

        (dmx,) = self.wait({0: 'h' if inv else 'l'})
        self.run_start = self.samplenum

        while True:
            # Seek for an interval with no state change with a length between
            # 88 and 1000000 us (BREAK).
            if self.state == 'FIND BREAK':
                (dmx,) = self.wait({0: 'f' if inv else 'r'})
                runlen = (self.samplenum - self.run_start) * self.sample_usec
                if runlen > 88 and runlen < 1000000:
                    self.putr([1, ['Break']])
                    self.state = 'MARK MAB'
                    self.channel = 0
                elif runlen >= 1000000:
                    # Error condition.
                    self.putr([10, ['Invalid break length']])
                else:
                    (dmx,) = self.wait({0: 'h' if inv else 'l'})
                    self.run_start = self.samplenum
            # Directly following the BREAK is the MARK AFTER BREAK.
            elif self.state == 'MARK MAB':
                self.run_start = self.samplenum
                (dmx,) = self.wait({0: 'r' if inv else 'f'})
                self.putr([2, ['MAB']])
                self.state = 'READ BYTE'
                self.channel = 0
                self.bit = 0
                self.aggreg = dmx
                self.run_start = self.samplenum
            # Mark and read a single transmitted byte
            # (start bit, 8 data bits, 2 stop bits).
            elif self.state == 'READ BYTE':
                bit_start = self.samplenum
                bit_end = self.run_start + (self.bit + 1) * self.skip_per_bit
                (dmx,) = self.wait({'skip': round(self.skip_per_bit/2)})
                bit_value = not dmx if inv else dmx

                if self.bit == 0:
                    self.byte = 0
                    self.put(bit_start, bit_end,
                             self.out_ann, [3, ['Start bit']])
                    if bit_value != 0:
                        # (Possibly) invalid start bit, mark but don't fail.
                        self.put(bit_start, bit_end,
                                 self.out_ann, [10, ['Invalid start bit']])
                elif self.bit >= 9:
                    self.put(bit_start, bit_end,
                             self.out_ann, [4, ['Stop bit']])
                    if bit_value != 1:
                        # Invalid stop bit, mark.
                        self.put(bit_start, bit_end,
                            self.out_ann, [10, ['Invalid stop bit']])
                        if self.bit == 10:
                            # On invalid 2nd stop bit, search for new break.
                            self.state = 'FIND BREAK'
                else:
                    # Label and process one bit.
                    self.put(bit_start, bit_end,
                             self.out_ann, [0, [str(bit_value)]])
                    self.byte |= bit_value << (self.bit - 1)

                # Label a complete byte.
                if self.state == 'READ BYTE' and self.bit == 10:
                    if self.channel == 0:
                        d = [5, ['Start code']]
                    else:
                        d = [6, ['Channel ' + str(self.channel)]]
                    self.put(self.run_start, bit_end, self.out_ann, d)
                    self.put(self.run_start + self.skip_per_bit,
                        bit_end - 2 * self.skip_per_bit,
                        self.out_ann, [9, [str(self.byte) + ' / ' + \
                        str(hex(self.byte))]])
                    # Continue by scanning the IFT.
                    self.channel += 1
                    self.run_start = self.samplenum
                    self.state = 'MARK IFT'

                self.bit += 1
                (dmx,) = self.wait({'skip': round(bit_end - self.samplenum)})
            # Mark the INTERFRAME-TIME between bytes / INTERPACKET-TIME between packets.
            elif self.state == 'MARK IFT':
                self.run_start = self.samplenum
                if self.channel > 512:
                    (dmx,) = self.wait({0: 'h' if inv else 'l'})
                    self.putr([8, ['Interpacket']])
                    self.state = 'FIND BREAK'
                    self.run_start = self.samplenum
                else:
                    if (not dmx if inv else dmx):
                        (dmx,) = self.wait({0: 'h' if inv else 'l'})
                        self.putr([7, ['Interframe']])
                    self.state = 'READ BYTE'
                    self.bit = 0
                    self.run_start = self.samplenum
