##
## This file is part of the libsigrokdecode project.
##
## Copyright (C) 2016 Vladimir Ermakov <vooon341@gmail.com>
## Copyright (C) 2019 DreamSourceLab <support@dreamsourcelab.com>
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
## along with this program; if not, see <http://www.gnu.org/licenses/>.
##

import sigrokdecode as srd
from functools import reduce

class SamplerateError(Exception):
    pass

class Decoder(srd.Decoder):
    api_version = 3
    id = 'rgb_led_ws281x'
    name = 'RGB LED (WS281x)'
    longname = 'RGB LED string decoder (WS281x)'
    desc = 'RGB LED string protocol (WS281x).'
    license = 'gplv3+'
    inputs = ['logic']
    outputs = []
    tags = ['Display', 'IC']
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
        self.reset()

    def reset(self):
        self.state = 'FIND RESET'
        self.samplerate = None
        self.ss_packet = None
        self.ss = None
        self.es = None
        self.bits = []

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
            self.ss_packet = samplenum

    def decode(self):
        if not self.samplerate:
            raise SamplerateError('Cannot decode without samplerate.')

        while True:
            if self.state == 'FIND RESET':
                self.wait({0: 'f'})
                self.ss = self.samplenum
                self.wait({0: 'r'})
                self.es = self.samplenum
                if ((self.es - self.ss) / self.samplerate > 50e-6):
                    self.state = 'RESET'
            elif self.state == 'RESET':
                self.put(self.ss, self.es, self.out_ann, [1, ['RESET', 'RST', 'R']])
                self.bits = []
                self.ss = self.samplenum
                self.ss_packet = self.samplenum
                self.wait({0: 'f'})
                self.state = 'BIT FALLING'
            elif self.state == 'BIT FALLING':
                self.es = self.samplenum
                self.wait({0: 'r'})
                if ((self.es - self.ss) / self.samplerate > 50e-6):
                    self.ss = self.es
                    self.es = self.samplenum
                    self.state = 'RESET'
                else:
                    self.state = 'BIT RISING'
            elif self.state == 'BIT RISING':
                period = self.samplenum - self.ss
                duty = self.es - self.ss
                # Ideal duty for T0H: 33%, T1H: 66%.
                bit_ = (duty / period) > 0.5
                
                self.put(self.ss, self.samplenum, self.out_ann,
                         [0, ['%d' % bit_]])
                
                self.bits.append(bit_)
                self.handle_bits(self.samplenum)

                self.ss = self.samplenum
                self.wait({0: 'f'})
                self.state = 'BIT FALLING'
