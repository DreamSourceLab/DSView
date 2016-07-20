##
## This file is part of the libsigrokdecode project.
##
## Copyright (C) 2012 Uwe Hermann <uwe@hermann-uwe.de>
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
    id = 'mlx90614'
    name = 'MLX90614'
    longname = 'Melexis MLX90614'
    desc = 'Infrared Thermometer protocol.'
    license = 'gplv2+'
    inputs = ['i2c']
    outputs = ['mlx90614']
    annotations = (
        ('celsius', 'Temperature in degrees Celsius'),
        ('kelvin', 'Temperature in Kelvin'),
    )

    def __init__(self):
        self.state = 'IGNORE START REPEAT'
        self.data = []

    def start(self):
        self.out_ann = self.register(srd.OUTPUT_ANN)

    def putx(self, data):
        self.put(self.ss, self.es, self.out_ann, data)

    # Quick hack implementation! This needs to be improved a lot!
    def decode(self, ss, es, data):
        cmd, databyte = data

        # State machine.
        if self.state == 'IGNORE START REPEAT':
            if cmd != 'START REPEAT':
                return
            self.state = 'IGNORE ADDRESS WRITE'
        elif self.state == 'IGNORE ADDRESS WRITE':
            if cmd != 'ADDRESS WRITE':
                return
            self.state = 'GET TEMPERATURE'
        elif self.state == 'GET TEMPERATURE':
            if cmd != 'DATA WRITE':
                return
            if len(self.data) == 0:
                self.data.append(databyte)
                self.ss = ss
            elif len(self.data) == 1:
                self.data.append(databyte)
                self.es = es
            else:
                kelvin = (self.data[0] | (self.data[1] << 8)) * 0.02
                celsius = kelvin - 273.15
                self.putx([0, ['Temperature: %3.2f °C' % celsius]])
                self.putx([1, ['Temperature: %3.2f K' % kelvin]])
                self.state = 'IGNORE START REPEAT'
                self.data = []
