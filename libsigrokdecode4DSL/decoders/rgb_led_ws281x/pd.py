##
## This file is part of the libsigrokdecode project.
##
## Copyright (C) 2016 Vladimir Ermakov <vooon341@gmail.com>
##
## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 3 of the License, or
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
from functools import reduce

class SamplerateError(Exception):
    pass

class Decoder(srd.Decoder):
    api_version = 2
    id = 'rgb_led_ws281x'
    name = 'RGB LED (WS281x)'
    longname = 'RGB LED string decoder (WS281x)'
    desc = 'RGB LED string protocol (WS281x).'
    license = 'gplv3+'
    inputs = ['logic']
    outputs = ['rgb_led_ws281x']
    channels = (
        {'id': 'din', 'name': 'DIN', 'desc': 'DIN data line'},
    )
    annotations = (
        ('bit', 'Bit'),
        ('reset', 'RESET'),
        ('rgb', 'RGB'),
    )
    annotation_rows = (
        ('bit', 'Bits', (0, 1)),
        ('rgb', 'RGB', (2,)),
    )

    def __init__(self):
        self.samplerate = None
        self.oldpin = None
        self.ss_packet = None
        self.ss = None
        self.es = None
        self.bits = []
        self.inreset = False

    def start(self):
        self.out_ann = self.register(srd.OUTPUT_ANN)

    def metadata(self, key, value):
        if key == srd.SRD_CONF_SAMPLERATE:
            self.samplerate = value

    def handle_bits(self, samplenum):
        if len(self.bits) == 24:
            grb = reduce(lambda a, b: (a << 1) | b, self.bits)
            rgb = (grb & 0xff0000) >> 8 | (grb & 0x00ff00) << 8 | (grb & 0x0000ff)
            self.put(self.ss_packet, samplenum, self.out_ann,
                     [2, ['#%06x' % rgb]])
            self.bits = []
            self.ss_packet = None

    def decode(self, ss, es, data):
        if not self.samplerate:
            raise SamplerateError('Cannot decode without samplerate.')

        for (samplenum, (pin, )) in data:
            data.itercnt += 1
            if self.oldpin is None:
                self.oldpin = pin
                continue

            # Check RESET condition (manufacturer recommends 50 usec minimal,
            # but real minimum is ~10 usec).
            if not self.inreset and not pin and self.es is not None and \
                    (samplenum - self.es) / self.samplerate > 50e-6:

                # Decode last bit value.
                tH = (self.es - self.ss) / self.samplerate
                bit_ = True if tH >= 625e-9 else False

                self.bits.append(bit_)
                self.handle_bits(self.es)

                self.put(self.ss, self.es, self.out_ann, [0, ['%d' % bit_]])
                self.put(self.es, samplenum, self.out_ann,
                         [1, ['RESET', 'RST', 'R']])

                self.inreset = True
                self.bits = []
                self.ss_packet = None
                self.ss = None

            if not self.oldpin and pin:
                # Rising edge.
                if self.ss and self.es:
                    period = samplenum - self.ss
                    duty = self.es - self.ss
                    # Ideal duty for T0H: 33%, T1H: 66%.
                    bit_ = (duty / period) > 0.5

                    self.put(self.ss, samplenum, self.out_ann,
                             [0, ['%d' % bit_]])

                    self.bits.append(bit_)
                    self.handle_bits(samplenum)

                if self.ss_packet is None:
                    self.ss_packet = samplenum

                self.ss = samplenum

            elif self.oldpin and not pin:
                # Falling edge.
                self.inreset = False
                self.es = samplenum

            self.oldpin = pin
