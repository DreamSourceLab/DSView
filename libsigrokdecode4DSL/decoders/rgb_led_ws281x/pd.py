##
## This file is part of the libsigrokdecode project.
##
## Copyright (C) 2016 Vladimir Ermakov <vooon341@gmail.com>
## Copyright (C) 2021 Michael Miller <makuna@live.com>
## Copyright (C) 2024 DreamSourceLab <support@dreamsourcelab.com>

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

##
##  2024/7/8 DreamSourceLab : default color order update 
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
        {'id': 'din', 'name': 'DIN', 'desc': 'DIN data line', 'idn':'dec_rgb_led_ws281x_chan_din'},
    )
    options = (
    	{'id': 'default_color_order', 'desc': 'Default Color Order', 'default': 'GRB',
            'values': ( 'GRB', 'RGB', 'BRG', 'RBG', 'BGR', 'GRBW', 'RGBW', 'WRGB', 'LBGR', 'LGRB', 'LRGB', 'LRBG', 'LGBR', 'LBRG')
            , 'idn':'dec_rgb_led_ws281x_opt_default_color_order'},
        {'id': 'view_color_order', 'desc': 'View Color Order', 'default': 'GRB',
            'values': ( 'GRB', 'RGB', 'BRG', 'RBG', 'BGR', 'GRBW', 'RGBW', 'WRGB', 'LBGR', 'LGRB', 'LRGB', 'LRBG', 'LGBR', 'LBRG')
            , 'idn':'dec_rgb_led_ws281x_opt_view_color_order'},
        {'id': 'polarity', 'desc': 'Polarity', 'default': 'normal',
            'values': ('normal', 'inverted'), 'idn':'dec_rgb_led_ws281x_opt_polarity'},
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

            default_val = 0
            view_val = 0

            if self.colorsize == 24:
                for i in range(3):
                    default_index = self.options['default_color_order'].find(self.options['view_color_order'][i])

                    if default_index == 0:
                        default_val = (elems & 0xff0000) >> 16
                    elif default_index == 1:
                        default_val = (elems & 0x00ff00) >> 8
                    elif default_index == 2:
                        default_val = (elems & 0x0000ff) 

                    if i == 0:
                        view_val = (default_val << 16) | view_val
                    elif i == 1:
                        view_val = (default_val << 8) | view_val
                    elif i == 2:
                        view_val = default_val | view_val
                        
                    self.put(self.ss_packet, samplenum, self.out_ann,[2, ['%s#%06x' % (self.options['view_color_order'] , view_val)]])

            else:
                for i in range(4):
                    default_index = self.options['default_color_order'].find(self.options['view_color_order'][i])

                    if default_index == 0:
                        default_val = (elems & 0xff000000) >> 24
                    elif default_index == 1:
                        default_val = (elems & 0x00ff0000) >> 16
                    elif default_index == 2:
                        default_val = (elems & 0x0000ff00) >> 8
                    elif default_index == 3:
                        default_val = (elems & 0x000000ff) 

                    if i == 0:
                        view_val = (default_val << 24) | view_val
                    elif i == 1:
                        view_val = (default_val << 16) | view_val
                    elif i == 2:
                        view_val = (default_val << 8) | view_val
                    elif i == 3:
                        view_val = default_val | view_val
                        
                    self.put(self.ss_packet, samplenum, self.out_ann,[2, ['%s#%08x' % (self.options['view_color_order'] , view_val)]])



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

    def end(self):
        if self.state == 'BIT FALLING':
            self.check_bit_(self.last_samplenum)
            self.put(self.ss, self.es, self.out_ann,
                        [0, ['%d' % self.bit_]])
            self.bits.append(self.bit_)
            self.handle_bits(self.es)


    def decode(self):
        if not self.samplerate:
            raise SamplerateError('Cannot decode without samplerate.')
        if len(self.options['default_color_order']) != len(self.options['view_color_order']):
            raise Exception('default color order len must equal to view color order len')
        
        if len(self.options['default_color_order']) == 4:
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
