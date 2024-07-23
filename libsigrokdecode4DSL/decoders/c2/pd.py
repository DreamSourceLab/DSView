##
## This file is part of the libsigrokdecode project.
##
## Copyright (C) 2024 DreamSourceLab <support@dreamsourcelab.com>
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

##
##  2024/7/5 DreamSourceLab : Read data only when the clock line is high
##

import sigrokdecode as srd

'''
'''

class ChannelError(Exception):
    pass

class Decoder(srd.Decoder):
    api_version = 3
    id = 'C2'
    name = 'C2 interface'
    longname = 'Silabs C2 Interface'
    desc = 'Half-duplex, synchronous, serial bus.'
    license = 'gplv2+'
    inputs = ['logic']
    outputs = ['C2']
    tags = ['Embedded/mcu']
    channels = (
        {'id': 'c2ck', 'type': 0, 'name': 'c2ck', 'desc': 'Clock', 'idn':'dec_c2_chan_c2ck'},
        {'id': 'c2d', 'type': 0, 'name': 'c2d', 'desc': 'Data', 'idn':'dec_c2_chan_c2d'},
    )
    optional_channels = ()
    annotations = (
        ('106', 'raw-Data', 'raw data'),
        ('106', 'c2-data', 'c2 data'),
        ('warnings', 'Warnings'),
    )
    annotation_rows = (
        ('raw-Data', 'raw data', (0,)),
        ('c2-data', 'c2 data', (1,)),
        ('warnings', 'Warnings', (2,)),
    )

    def __init__(self):
        self.reset()

    def reset(self):
        self.samplerate = None
        self.state= 'reset'
        self.bitcount = 0
        self.c2_data =  0
        self.data=0
        self.have_c2ck = self.have_c2d = None
        self.ins= None

        self.start_first_sample = 0
        self.data_first_sample = 0

        self.data_len=0
        self.remain_data=0

    def start(self):
        self.out_ann = self.register(srd.OUTPUT_ANN)

    def metadata(self, key, value):
        if key == srd.SRD_CONF_SAMPLERATE:
            self.samplerate = value

    def decode(self):
        if not self.has_channel(0):
            raise ChannelError('CLK pin required.')
        self.have_c2d = self.has_channel(1)
        if not self.have_c2d:
            raise ChannelError('C2D pins required.')
        #
        tf=0
        tr=0
        
        while True:
            (c2ck, c2d) = self.wait({ 0 : 'e' })
            if c2ck == 0:
                tf = self.samplenum
            else:
                tr = self.samplenum

                interval = (tr - tf) * 1000 * 1000 / self.samplerate #us

                if interval > 20:
                    self.put(tf, tr, self.out_ann, [0, [ 'Reset','R']])
                    self.state = 'start'

                elif self.state == 'start':
                    self.put(tf, tr, self.out_ann, [0, [ 'Start','S']])

                    self.start_first_sample = tf

                    self.state = 'ins'
                    ss = tr        
                    self.ins = 0
                    self.bitcount = 0

                elif self.state == 'ins':
                    self.ins |= c2d << self.bitcount
                    self.bitcount += 1
                    
                    if self.bitcount >= 2:
                        self.put(ss, self.samplenum, self.out_ann, [0, [ '%1d'%self.ins]])

                        if self.ins == 0 :
                            self.state = 'Data Read Len'
                            self.data_len = 0
                            self.remain_data = 0
                        elif self.ins == 1:
                            self.state = 'Data Write Len'
                            self.data_len = 0
                            self.remain_data = 0
                        elif self.ins == 2:
                            self.state = 'Address Read'
                        elif self.ins == 3:
                            self.state = 'Address Write'
                        
                        ss = tr        
                        self.c2_data = 0
                        self.bitcount = 0

                elif self.state == 'Data Read Len':
                    self.c2_data |= c2d <<self.bitcount
                    self.bitcount += 1

                    if self.bitcount >= 2:
                        self.put(ss, self.samplenum, self.out_ann, [0, [ '%01d'%self.c2_data]])

                        self.state = 'Read Wait'
                        self.data_len = self.c2_data + 1
                        self.remain_data = self.data_len

                        ss = tr
 
                elif self.state == 'Read Wait':
                    self.put(ss, self.samplenum, self.out_ann, [0, ['Wait','W']])
                    self.state = 'Data Read'
                    
                    self.data = 0
                    ss = tr
                    self.c2_data = 0
                    self.bitcount = 0 

                elif self.state == 'Data Read':
                    self.c2_data |= c2d << self.bitcount
                    self.bitcount += 1

                    if self.bitcount >= 8:
                        self.put(ss, tr, self.out_ann, [0, ['%02X' % self.c2_data]])

                        self.data |= self.c2_data << ((self.data_len - self.remain_data) * 8)

                        ss = tr
                        self.c2_data = 0
                        self.bitcount = 0 

                        self.remain_data -= 1
                        if self.remain_data == 0:
                            self.state = 'End'

                elif self.state == 'Data Write Len':
                    self.c2_data |= c2d << self.bitcount
                    self.bitcount += 1
                    
                    if self.bitcount >= 2:
                        self.put(ss, self.samplenum, self.out_ann, [0, ['%01d'%self.c2_data]])

                        self.state='Data Write'
                        self.data_len = self.c2_data + 1
                        self.remain_data = self.data_len

                        self.data = 0
                        ss = tr
                        self.c2_data = 0
                        self.bitcount = 0

                elif self.state == 'Data Write':
                    self.c2_data |= c2d <<self.bitcount
                    self.bitcount += 1

                    if self.bitcount >= 8:
                        self.put(ss, tr, self.out_ann, [0, ['%02X' % self.c2_data]])

                        self.data |= self.c2_data << ((self.data_len - self.remain_data) * 8)

                        ss = tr
                        self.c2_data = 0
                        self.bitcount = 0 

                        self.remain_data -= 1

                        if self.remain_data ==0:
                            self.state='Write Wait'
                elif self.state == 'Write Wait':
                    self.put(ss, self.samplenum, self.out_ann, [0, ['Wait','W']])
                    self.state = 'End'

                    ss = tr

                elif self.state == 'Address Write':
                    self.c2_data |= c2d << self.bitcount
                    self.bitcount += 1

                    if self.bitcount >= 8:
                        self.put(ss, self.samplenum, self.out_ann, [0, ['%02X' % self.c2_data]])

                        self.state = 'End'
                        ss = tr

                elif self.state == 'Address Read':
                    self.c2_data |= c2d << self.bitcount
                    self.bitcount += 1

                    if self.bitcount >= 8:
                        self.put(ss, self.samplenum, self.out_ann, [0, ['%02X' % self.c2_data]])
                        
                        self.state = 'End'
                        ss = tr

                elif self.state == 'End':
                    self.put(ss, self.samplenum, self.out_ann, [0, [ 'End','E']])

                    if self.ins == 0:
                        self.put(self.start_first_sample, self.samplenum, self.out_ann, [1, [ 'ReadData(%01d)=0x%02X'%(self.data_len,self.data)]])
                    elif self.ins == 1:
                        self.put(self.start_first_sample, self.samplenum, self.out_ann, [1, [ 'WriteData(0x%02X,%01d)'%(self.data,self.data_len)]])
                    elif self.ins == 2:
                        self.put(self.start_first_sample, self.samplenum, self.out_ann, [1, [ 'ReadAddress()=0x%02X'%self.c2_data]])
                    elif self.ins == 3:
                        self.put(self.start_first_sample, self.samplenum, self.out_ann, [1, [ 'WriteAddress(0x%02X)'%self.c2_data]])

                    self.state = 'start'

                        
