##
## This file is part of the libsigrokdecode project.
##
## Copyright (C) 2016 Vladimir Ermakov <vooon341@gmail.com>
## Copyright (C) 2019 DreamSourceLab <support@dreamsourcelab.com>
## Copyright (C) 2021 Michael Miller <makuna@live.com>
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
    name = 'RGB LED WS2812+'
    longname = 'RGB LED color decoder'
    desc = 'Decodes colors from bus pulses for single wire RGB leds like APA106, WS2811, WS2812, WS2813, SK6812, TM1829, TM1814, and TX1812.'
    license = 'gplv3+'
    inputs = ['logic']
    outputs = []
    tags = ['Display', 'IC']
    channels = (
        {'id': 'din', 'name': 'DIN', 'desc': 'DIN data line'},
    )
    options = (
        {'id': 'colors', 'desc': 'Colors', 'default': 'GRB',
            'values': ( 'GRB', 'RGB', 'BRG', 'RBG', 'BGR', 'GRBW', 'RGBW', 'WRGB', 'LBGR', 'LGRB', 'LRGB', 'LRBG', 'LGBR', 'LBRG')},
        {'id': 'polarity', 'desc': 'Polarity', 'default': 'normal',
            'values': ('normal', 'inverted')},
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
        self.bit_ = None
        self.colorsize = None

    def start(self):
        self.out_ann = self.register(srd.OUTPUT_ANN)


    def metadata(self, key, value):
        if key == srd.SRD_CONF_SAMPLERATE:
            self.samplerate = value

    def handle_bits(self, samplenum):
         if len(self.bits) == self.colorsize:
             elems = reduce(lambda a, b: (a << 1) | b, self.bits)
             if self.colorsize == 24:
                if self.options['colors'] == 'GRB':
                    rgb = (elems & 0xff0000) >> 8 | (elems & 0x00ff00) << 8 | (elems & 0x0000ff)
                elif self.options['colors'] == 'RGB':
                    rgb = elems
                elif self.options['colors'] == 'BRG':
                    rgb = (elems & 0xff0000) >> 16 | (elems & 0x00ffff) << 8
                elif self.options['colors'] == 'RBG':
                    rgb = (elems & 0xff0000) | (elems & 0x00ff00) >> 8 | (elems & 0x0000ff) << 8
                elif self.options['colors'] == 'BGR':
                    rgb = (elems & 0xff0000) >> 16 | (elems & 0x00ff00) | (elems & 0x0000ff) << 16

                self.put(self.ss_packet, samplenum, self.out_ann,
                         [2, ['RGB#%06x' % rgb]])
             else:
                if self.options['colors'] == 'GRBW':
                    rgb = (elems & 0xff000000) >> 16 | (elems & 0x00ff0000) | (elems & 0x0000ff00) >> 8
                    w = (elems & 0x000000ff)
                elif self.options['colors'] == 'RGBW':
                    rgb = (elems & 0xffffff00) >> 8
                    w = (elems & 0x000000ff)
                elif self.options['colors'] == 'WRGB':
                    rgb = (elems & 0x00ffffff)
                    w = (elems & 0xff000000) >> 32
                elif self.options['colors'] == 'LBGR':
                    rgb = (elems & 0x0000ff00) | (elems & 0x00ff0000) >> 16 | (elems & 0x000000ff) << 16
                    w = (elems & 0xff000000) >> 32
                elif self.options['colors'] == 'LGRB':
                    rgb = (elems & 0x000000ff) | (elems & 0x00ff0000) >> 8 | (elems & 0x0000ff00) << 8
                    w = (elems & 0xff000000) >> 32
                elif self.options['colors'] == 'LRGB':
                    rgb = (elems & 0x00ffffff)
                    w = (elems & 0xff000000) >> 32
                elif self.options['colors'] == 'LRBG':
                    rgb = (elems & 0x00ff0000) | (elems & 0x0000ff00) >> 8 | (elems & 0x000000ff) << 8
                    w = (elems & 0xff000000) >> 32
                elif self.options['colors'] == 'LGBR':
                    rgb = (elems & 0x00ffff00) >> 8 | (elems & 0x000000ff) << 16
                    w = (elems & 0xff000000) >> 32
                elif self.options['colors'] == 'LBRG':
                    rgb = (elems & 0x00ff0000) >> 16 | (elems & 0x0000ffff) << 8
                    w = (elems & 0xff000000) >> 32

                self.put(self.ss_packet, samplenum, self.out_ann,
                         [2, ['RGB#%06x #%02x' % (rgb, w)]])

             self.bits = []
             self.ss_packet = samplenum

    def check_bit_(self, samplenum):
        period = samplenum - self.ss
        tH_samples = self.es - self.ss
        tH = tH_samples / self.samplerate
        if tH >= 625e-9:
            self.bit_ = True
        else:
            # Ideal duty for T0H: 33%, T1H: 66%.
            self.bit_ = (tH_samples / period) > 0.5


    def decode(self):
        if not self.samplerate:
            raise SamplerateError('Cannot decode without samplerate.')

        if len(self.options['colors']) == 4:
            self.colorsize = 32
        else:
            self.colorsize = 24

        while True:
            if self.state == 'FIND RESET':
                self.wait({0: 'l' if self.options['polarity'] == 'normal' else 'h'})
                self.ss = self.samplenum
                self.wait({0: 'e'})
                self.es = self.samplenum
                if ((self.es - self.ss) / self.samplerate > 50e-6):
                    self.state = 'RESET'
                elif ((self.es - self.ss) / self.samplerate > 3e-6):
                    self.bits = []
                    self.ss = self.samplenum
                    self.ss_packet = self.samplenum
                    self.wait({0: 'e'})
                    self.state = 'BIT FALLING'
            elif self.state == 'RESET':
                self.put(self.ss, self.es, self.out_ann, [1, ['RESET', 'RST', 'R']])
                self.bits = []
                self.ss = self.samplenum
                self.ss_packet = self.samplenum
                self.wait({0: 'e'})
                self.state = 'BIT FALLING'
            elif self.state == 'BIT FALLING':
                self.es = self.samplenum
                self.wait({0: 'e'})
                if ((self.samplenum - self.es) / self.samplerate > 50e-6):
                    self.check_bit_(self.samplenum)
                    self.put(self.ss, self.es, self.out_ann,
                             [0, ['%d' % self.bit_]])
                    self.bits.append(self.bit_)
                    self.handle_bits(self.es)

                    self.ss = self.es
                    self.es = self.samplenum
                    self.state = 'RESET'
                else:
                    self.state = 'BIT RISING'
            elif self.state == 'BIT RISING':
                self.check_bit_(self.samplenum)
                self.put(self.ss, self.samplenum, self.out_ann,
                         [0, ['%d' % self.bit_]])
                self.bits.append(self.bit_)
                self.handle_bits(self.samplenum)

                self.ss = self.samplenum
                self.wait({0: 'e'})
                self.state = 'BIT FALLING'
