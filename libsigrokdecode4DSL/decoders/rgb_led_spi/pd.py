##
## This file is part of the libsigrokdecode project.
##
## Copyright (C) 2014 Matt Ranostay <mranostay@gmail.com>
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

class Decoder(srd.Decoder):
    api_version = 2
    id = 'rgb_led_spi'
    name = 'RGB LED (SPI)'
    longname = 'RGB LED string decoder (SPI)'
    desc = 'RGB LED string protocol (RGB values clocked over SPI).'
    license = 'gplv2+'
    inputs = ['spi']
    outputs = ['rgb_led_spi']
    annotations = (
        ('rgb', 'RGB values'),
    )

    def __init__(self):
        self.ss_cmd, self.es_cmd = 0, 0
        self.mosi_bytes = []

    def start(self):
        self.out_ann = self.register(srd.OUTPUT_ANN)

    def putx(self, data):
        self.put(self.ss_cmd, self.es_cmd, self.out_ann, data)

    def decode(self, ss, es, data):
        ptype, mosi, miso = data

        # Only care about data packets.
        if ptype != 'DATA':
            return
        self.ss, self.es = ss, es

        if len(self.mosi_bytes) == 0:
            self.ss_cmd = ss
        self.mosi_bytes.append(mosi)

        # RGB value == 3 bytes
        if len(self.mosi_bytes) != 3:
            return

        red, green, blue = self.mosi_bytes
        rgb_value = int(red) << 16 | int(green) << 8 | int(blue)

        self.es_cmd = es
        self.putx([0, ['#%.6x' % rgb_value]])
        self.mosi_bytes = []
