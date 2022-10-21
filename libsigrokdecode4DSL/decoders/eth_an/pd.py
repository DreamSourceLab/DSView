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
        {'id': 'dp', 'name': 'TX+', 'desc': 'ETH TX+ signal', 'idn':'dec_eth_an_chan_dp'},
    )
    annotations = (
        ('data', 'FLP data'),
        ('format', 'format describe'),
        ('bitd', 'Bit desc'),
        ('bit', 'Bit'),
        ('NLP', 'Normal link pulses'),
    )
    annotation_rows = (
        ('data',  'Data', (0,)),
        ('format',  'Format', (1,)),
        ('bitd',  'Bit desc', (2,)),
        ('bit', 'Bit', (3,)),
        ('NLP', 'NLP', (4,)),
    )

    def __init__(self):
        self.reset()

    def reset(self):
        self.ss = self.es = 0
        self.pre_ss = self.pre_es = 0
        self.last_vaild_ss = 0
        self.samplerate = None
        self.samplenum = 0
        self.hex = 0
        self.pre_hex = 0
        self.index = 0
        self.data_list = []
        self.state = 'base page'
        
    def start(self):
        self.out_python = self.register(srd.OUTPUT_PYTHON)
        self.out_ann = self.register(srd.OUTPUT_ANN)
        self.out_binary = self.register(srd.OUTPUT_BINARY)

    def metadata(self, key, value):
        if key == srd.SRD_CONF_SAMPLERATE:
            self.samplerate = value

    def changeState(self):
        if self.pre_hex != self.hex:
            if self.state == 'base page':
                if ((self.hex>>14)&0x3) == 0x3:
                    self.state = 'base page ack'
            elif self.state == 'base page ack':
                self.state = 'next page'
            elif self.state == 'next page':
                if ((self.hex>>14)&0x3) == 0x1:
                    self.state = 'next page ack'
            elif self.state == 'next page ack':
                self.state = 'base page'

    def decodeTiming(self):
        self.wait({0: 'r'})
        self.ss = self.samplenum
        self.wait({0: 'e'})
        self.es = self.samplenum

        length = float(self.es - self.ss)/self.samplerate
        if length <= 1.0e-5 and length >= 1.0e-6: # 1us ~ 10us 
            self.put(self.ss, self.es, self.out_ann, [4, ['NLP']])
            temp_length = self.ss - self.last_vaild_ss
            length = float(temp_length)/self.samplerate
            self.last_vaild_ss = self.ss
            if length <= 7.0e-5 and length >= 6.0e-5: # 60us ~ 70us
                self.hex = self.hex|0x1<<self.index
                self.data_list.append({'start':self.ss, 
                                    'end':self.es,
                                    'Pre start':self.pre_ss, 
                                    'Pre end':self.pre_es})
                self.index = self.index + 1
                self.last_vaild_ss = 0
            elif length <= 1.4e-4 and length >= 1.2e-4: # 120us ~ 140us
                self.data_list.append({'start':self.ss-(temp_length>>1),
                                    'end':self.es-(temp_length>>1),
                                    'Pre start':self.pre_ss,
                                    'Pre end':self.pre_es})
                self.index = self.index + 1
            self.pre_ss = self.ss
            self.pre_es = self.es

    def decodeBasePage(self):
        base_page_ta_dict = {
            5:'10BaseT-HD',
            6:'10BaseT-FD',
            7:'100BaseTX-HD',
            8:'100BaseTX-FD',
            9:'100BaseT4',
            10:'FC',
            11:'AsyFC',
            12:'Reserved',
            13:'RF',
            14:'ACK',
            15:'NP',
        }
        if (self.hex&0x1f) == 0x1:
            type_desc = '802.3'
        elif (self.hex&0x1f) == 0x2:
            type_desc = '802.9'
        else:
            type_desc = 'unknow'
        for i in range(16):
            self.put(self.data_list[i]['Pre start'],
                        self.data_list[i]['end'], 
                        self.out_ann, 
                        [3, [str(hex(((self.hex>>i)&0x1)))]])
            if i in base_page_ta_dict.keys():
                self.put(self.data_list[i]['Pre start'],
                            self.data_list[i]['end'], 
                            self.out_ann, 
                            [2, [base_page_ta_dict[i]]])
        self.put(self.data_list[0]['Pre start'],
                self.data_list[15]['end'],
                self.out_ann,
                [0, ["base page:"+str(hex(self.hex))]])
        self.put(self.data_list[0]['Pre start'], 
                self.data_list[4]['end'], 
                self.out_ann, 
                [1, ["Selector field:"+str(hex(self.hex&0x1f))]])
        self.put(self.data_list[0]['Pre start'], 
                self.data_list[4]['end'], 
                self.out_ann, 
                [2, [type_desc]])
        self.put(self.data_list[5]['Pre start'],
                self.data_list[12]['end'], 
                self.out_ann, 
                [1, ["Technology ability field:"+str(hex(((self.hex>>5)&0xff)))]])
        self.put(self.data_list[13]['Pre start'],
                self.data_list[15]['end'], 
                self.out_ann, 
                [1, ["Other fields:"+str(hex(((self.hex>>13)&0x7)))]])

    def decodeNextPage(self):
        next_page_ta_dict = {
            0:'1000BaseT M/S CFG EN',
            1:'1000BaseT M/S CFG Vale',
            2:'Port type',
            3:'1000BaseT-FD',
            4:'1000BaseT-HD',
            5:'10GBaseT-FD',
            6:'10GBaseT M/S CFG EN',
            7:'10GBaseT M/S CFG Vale',
            8:'Reserved',
            9:'Reserved',
            10:'Reserved',
        }
        next_page_ot_dict = {
            11:'T',
            12:'ACK2',
            13:'MP',
            14:'ACK',
            15:'NP',
        }
        if ((self.hex>>13)&0x1) == 0x1:
            mp = "Technology ability field:"
        else :
            mp = "Unformatted code field:"
        for i in range(16):
            self.put(self.data_list[i]['Pre start'],
                    self.data_list[i]['end'], 
                    self.out_ann, 
                    [3, [str(hex(((self.hex>>i)&0x1)))]])
            if i in next_page_ot_dict.keys():
                self.put(self.data_list[i]['Pre start'],
                        self.data_list[i]['end'], 
                        self.out_ann, 
                        [2, [next_page_ot_dict[i]]])
            if mp == "Technology ability field:":
                if i in next_page_ta_dict.keys():
                    self.put(self.data_list[i]['Pre start'],
                            self.data_list[i]['end'], 
                            self.out_ann, 
                            [2, [next_page_ta_dict[i]]])
        self.put(self.data_list[0]['Pre start'],
                    self.data_list[15]['end'],
                    self.out_ann,
                    [0, ["next page:"+str(hex(self.hex))]])
        self.put(self.data_list[0]['Pre start'], 
                self.data_list[10]['end'], 
                self.out_ann, 
                [1, [mp+str(hex((self.hex&0x7ff)))]])
        if mp == "Unformatted code field:":
            self.put(self.data_list[0]['Pre start'], 
                        self.data_list[10]['end'], 
                        self.out_ann,
                        [2, ["Master-Slave seed value (MSB)"]])
        self.put(self.data_list[11]['Pre start'],
                self.data_list[15]['end'], 
                self.out_ann, 
                [1, ["Other fields:"+str(hex(((self.hex>>11)&0x1f)))]])

    def updateOnceDecode(self):
        self.pre_hex = self.hex
        self.data_list = []
        self.index = 0
        self.hex = 0

    def decode(self):
        while True:
            self.decodeTiming()
            if self.index == 16:
                self.changeState()
                if self.state == 'base page' or self.state == 'base page ack':
                    self.decodeBasePage()
                elif self.state == 'next page' or self.state == 'next page ack':
                    self.decodeNextPage()
                self.updateOnceDecode()
