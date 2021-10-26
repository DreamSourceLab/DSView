##
## This file is part of the libsigrokdecode project.
##
## Copyright (C) 2021 Quard <2014500726@smail.xtu.edu.cn>
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
    id = 'eth_auto_negotiation'
    name = 'ETH_AN'
    longname = 'ETH Auto Negotiation'
    desc = 'ETH Auto Negotiation protocol.'
    license = 'gplv2+'
    inputs = ['logic']
    outputs = ['eth_an']
    tags = ['PC']
    channels = (
        {'id': 'dp', 'name': 'TX+', 'desc': 'ETH TX+ signal'},
    )
    annotations = (
        ('data', 'FLP data'),
        ('format', 'format describe'),
        ('bit', 'Bit'),
        ('NLP', 'Normal link pulses'),
    )
    annotation_rows = (
        ('data',  'Data', (0,)),
        ('format',  'Format', (1,)),
        ('bit', 'Bit', (2,)),
        ('NLP', 'NLP', (3,)),
    )

    def __init__(self):
        self.reset()

    def reset(self):
        self.ss = self.es = 0
        self.last_ss = self.last_es = 0
        self.last_vaild_ss = 0
        self.samplerate = None
        self.samplenum = 0
        self.hex = 0

    def start(self):
        self.out_python = self.register(srd.OUTPUT_PYTHON)
        self.out_ann = self.register(srd.OUTPUT_ANN)
        self.out_binary = self.register(srd.OUTPUT_BINARY)

    def metadata(self, key, value):
        if key == srd.SRD_CONF_SAMPLERATE:
            self.samplerate = value

    def decode(self):
        index = 0
        data_list = []
        while True:
            self.wait({0: 'r'})
            self.ss = self.samplenum
            self.wait({0: 'e'})
            self.es = self.samplenum

            length = float(self.es - self.ss)/self.samplerate
            if length <= 1.0e-5 and length >= 1.0e-6: # 1us ~ 10us 
                self.put(self.ss, self.es, self.out_ann, [3, ['NLP']])
                temp_length = self.ss - self.last_vaild_ss
                length = float(temp_length)/self.samplerate
                self.last_vaild_ss = self.ss
                if length <= 7.0e-5 and length >= 6.0e-5: # 60us ~ 70us
                    self.put(self.last_ss, self.es, self.out_ann, [2, ['0x1']])
                    self.hex = self.hex|0x1<<index
                    data_list.append([self.ss, self.es,self.last_ss,self.last_es])
                    index = index + 1
                    self.last_vaild_ss = 0
                elif length <= 1.4e-4 and length >= 1.2e-4: # 120us ~ 140us
                    self.put(self.last_ss, self.es-(temp_length>>1), self.out_ann, [2, ['0x0']])
                    data_list.append([self.ss-(temp_length>>1), self.es-(temp_length>>1),self.last_ss,self.last_es])
                    index = index + 1
                self.last_ss = self.ss
                self.last_es = self.es

            if index == 16:
                self.put(data_list[0][2], data_list[15][1], self.out_ann, [0, ["Data:"+str(hex(self.hex))]])
                self.put(data_list[0][2], data_list[4][1], self.out_ann, [1, ["Selector field:"+str(hex(self.hex&0x1f))]])
                self.put(data_list[5][2], data_list[12][1], self.out_ann, [1, ["Technology ability field:"+str(hex(((self.hex>>5)&0xff)))]])
                self.put(data_list[13][2], data_list[15][1], self.out_ann, [1, ["Other fields:"+str(hex(((self.hex>>13)&0x7)))]])
                data_list = []
                index = 0
                self.hex = 0