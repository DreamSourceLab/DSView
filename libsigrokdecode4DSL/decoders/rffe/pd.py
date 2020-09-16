##
## This file is part of the libsigrokdecode project.
##
## Copyright (C) 2010-2016 Uwe Hermann <uwe@hermann-uwe.de>
## Copyright (C) 2020 DreamSourceLab <support@dreamsourcelab.com>
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

# TODO: Look into arbitration, collision detection, clock synchronisation, etc.
# TODO: Implement support for inverting SDATA/SCLK levels (0->1 and 1->0).
# TODO: Implement support for detecting various bus errors.

import sigrokdecode as srd

'''
OUTPUT_PYTHON format:

Packet:
[SSC,<Command Frame>, <Data Frame>]

SSC:Sequence Start Condition

<Command Frame>:
 - 'SA' (Slave Address)
 - 'COMMAND' (command Key)
 - 'BC' (Byte Count)
 - 'P' (Parity)
 - 'ADDRESS' (Register Address)
 - 'DATA WRITE0' (Register 0 Write Data)
<Data Frame>:
 - 'ADDRESS' (Register Address)
 - 'P' (Parity)
 - 'BP' (Bus Park)
 - 'DATA READ' (Data, read)
 - 'DATA WRITE' (Data, write)

 <Command Frame> : A Command Frame shall consist of a 4-bit Slave address field, an 8-bit command payload field, and a single parity bit.

 <Data Frame> : A Data or Address Frame shall consist of eight data bits or eight address bits, respectively, and a single parity bit. 
'''
# cmd: [annotation-type-index, long annotation, short annotation]
proto = {
    'SSC':             [0, 'Sequence Start Condition',       'SSC'],
    'SA':              [1, 'Slave Address',                   'SA'],
    'ERW':             [2, 'Extended Register Write',        'ERW'],
    'ERR':             [3, 'Extended Register Read',         'ERR'],
    'ERWL':            [4, 'Extended Register Write Long',  'ERWL'],
    'ERRL':            [5, 'Extended Register Read Long',   'ERRL'],
    'RW':              [6, 'Register Write',                  'RW'],
    'RR':              [7, 'Register Read',                   'RR'],
    'R0W':             [8, 'Register 0 Write',               'R0W'],
    'BC':              [9, 'Byte',                            'BC'],
    'P':               [10, 'Parity',                          'P'],
    'ADDRESS':         [11, 'Address',                         'A'],
    'BP':              [12, 'Bus Pack',                       'BP'],
    'DATA':            [13, 'Data ',                        'DATA'],
    'CMD_WARNINGS':    [14, 'Command Warnings',         'CMD_WARN'],
    'BIC':             [15, 'Bus Idle Condition ',           'BIC'],
    'BC_WARNINGS':     [16, 'BC Warnings',               'BC_WARN'],
    'IJE':             [17, 'Illegal Jump Edge',        'IJE_WAEN'],
    'PW':              [18, 'Parity warnings',            'P_WAEN'],
}

class Decoder(srd.Decoder):
    api_version = 3
    id = 'rffe'
    name = 'RFFE'
    longname = 'RF Front-End Control Interface'
    desc = 'Two-wire, single-master, serial bus.'
    license = 'gplv2+'
    inputs = ['logic']
    outputs = ['rffe']
    tags = ['Embedded/industrial']
    channels = (
        {'id': 'sclk', 'type': 8, 'name': 'SCLK', 'desc': 'Serial clock line'},
        {'id': 'sdata', 'type': 108, 'name': 'SDATA', 'desc': 'Serial data line'},
    )
    options = (
        {'id': 'error_display', 'desc': 'Error display options',
            'default': 'display', 'values': ('display', 'not_display')},
    )
    annotations = (
        ('7', 'ssc', 'Sequence Start Condition'),
        ('6', 'sa', 'Slave Address'),
        ('1', 'erw', 'Extended register write'),
        ('5', 'err', 'Extended register read'),
        ('0', 'erwl', 'Extended register write long'),
        ('112', 'errl', 'Extended register read long'),
        ('111', 'rw', 'Register write'),
        ('110', 'rr', 'Register read'),
        ('109', 'r0w', 'Register 0 write'),
        ('108', 'bc', 'Byte'),
        ('7', 'p', 'Parity'),
        ('75', 'address', 'Address'),
        ('70', 'bp', 'Bus pack'),
        ('65', 'data', 'DATA'),
        ('1000', 'Command warnings', 'Command warnings'),
        ('50', 'Bus Idle Condition ', 'Bus Idle Condition '),
        ('1000', 'BC warnings', 'BC warnings'),
        ('1000', 'Illegal Jump Edge', 'IJE_WAEN'),
        ('1000', 'Parity warnings', 'Parity warnings'),
    )
    annotation_rows = (
        ('command-data', 'Command/Data', (0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 , 11, 12, 13 ,15)),
        ('warnings', 'Warnings', (14,16,17,18,)),
    )

    def __init__(self):
        self.reset()

    def reset(self):
        self.samplerate = None
        self.ss = self.es = -1
        self.bitcount = 0
        self.databyte = 0
        self.state = 'FIND SSC'
        self.extended = -1
        self.cmdkey = 'NULL'
        self.BC = 0
        self.bits = 0
        self.Pcount = 0
        self.BPcount = 0
        self.ADDcount = 0
        self.BPss = 0
        self.SSCs = 0
        self.Pes = 0
        self.sdata = -1
        self.Pdata = -1
        self.parity = False

    def metadata(self, key, value):
        if key == srd.SRD_CONF_SAMPLERATE:
            self.samplerate = value

    def start(self):
        self.out_ann = self.register(srd.OUTPUT_ANN)
        self._display = 1 if self.options['error_display'] == 'display' else 0

    def putx(self, data):
        self.put(self.ss, self.es, self.out_ann, data)

    def Parity(self):
        self.parity = True
        if self.Pcount == 1 :
            if self.cmdkey == 'ERW' :
                self.Pdata = self.Pdata + (0*(2**(self.Pkey+1)))
            if self.cmdkey == 'ERR' :
                self.Pdata = self.Pdata + (2*(2**(self.Pkey+1)))
            if self.cmdkey == 'ERWL' :
                self.Pdata = self.Pdata + (6*(2**(self.Pkey+1)))
            if self.cmdkey == 'ERRL' :
                self.Pdata = self.Pdata + (7*(2**(self.Pkey+1)))                 
            if self.cmdkey == 'RW' :
                self.Pdata = self.Pdata + (2*(2**(self.Pkey+1)))                 
            if self.cmdkey == 'RR' :
                self.Pdata = self.Pdata + (3*(2**(self.Pkey+1)))                 
            if self.cmdkey == 'R0W' :
                self.Pdata = self.Pdata + (1*(2**(self.Pkey+1))) 
        while self.Pdata :
            self.parity = not self.parity
            self.Pdata = self.Pdata & (self.Pdata - 1)
        self.Pdata = self.Pkey = 0
                
            
    def handle_BP(self,cmd,state):

        self.ss = self.Pes
        self.es = self.samplenum        
        self.putx([proto[cmd][0], proto[cmd][1:]])
        self.state = state

    def handle(self,cmd,state,key,key0):
        key1 = key
        if key > 7 :
            key = 7
        if self.bitcount == 0:
            self.DATAss = self.samplenum
        
        if self.bitcount < key:
            while True :
                if self._display :
                    (sclk, sdata) = self.wait([{0: 'f'},{0: 'l', 1: 'e'}])
                    if (self.matched & (0b1 << 0)):
                        self.databyte <<= 1
                        self.databyte |= sdata
                        break
                    if (self.matched & (0b1 << 1)):
                        self.ss = self.samplenum
                        (sclk, sdata) = self.wait([{0: 'f'},{0: 'l', 1: 'e'}])
                        if (self.matched & (0b1 << 0)):
                            self.es = self.samplenum
                            self.putx([proto['IJE'][0], proto['IJE'][1:]])
                            self.databyte <<= 1
                            self.databyte |= sdata
                            break
                        if (self.matched & (0b1 << 1)):
                            self.es = self.samplenum                        
                            self.putx([proto['IJE'][0], proto['IJE'][1:]])
                else :
                    (sclk, sdata) = self.wait({0: 'f'})
                    self.databyte <<= 1
                    self.databyte |= sdata
                    break

            self.bitcount += 1
            return
        
        while True :
            if self._display :
                (sclk, sdata) = self.wait([{0: 'f'},{0: 'l', 1: 'e'}])
                if (self.matched & (0b1 << 0)):
                    self.databyte <<= 1
                    self.databyte |= sdata
                    break
                if (self.matched & (0b1 << 1)):
                    self.ss = self.samplenum
                    (sclk, sdata) = self.wait([{0: 'f'},{0: 'l', 1: 'e'}])
                    if (self.matched & (0b1 << 0)):
                        self.es = self.samplenum
                        self.putx([proto['IJE'][0], proto['IJE'][1:]])
                        self.databyte <<= 1
                        self.databyte |= sdata
                        break
                    if (self.matched & (0b1 << 1)):
                        self.es = self.samplenum                        
                        self.putx([proto['IJE'][0], proto['IJE'][1:]])
            else :
                (sclk, sdata) = self.wait({0: 'f'})
                self.databyte <<= 1
                self.databyte |= sdata
                break
        self.wait({0 : 'r'})
        d = self.databyte
        self.ss = self.DATAss
        self.es = self.samplenum
        if cmd != 'P':
            self.Pdata = d
            self.Pkey = key
        if cmd == 'BC':
            self.BC = d
            if self.cmdkey == 'ERW' or self.cmdkey == 'ERR' :
                if self.BC < 4 or self.BC > 16 :
                    self.putx([proto['BC_WARNINGS'][0], proto['BC_WARNINGS'][1:]])
                    self.init()
                    return 
            else :
                if self.BC < 1 or self.BC > 8 :
                    self.putx([proto['BC_WARNINGS'][0], proto['BC_WARNINGS'][1:]])
                    self.init()
                    return
        if cmd == 'P':
            self.Pes = self.samplenum
            if self._display :
                self.Parity()
                if self.parity != d :
                    self.putx([proto['PW'][0], proto['PW'][1:]]) 
            self.putx([proto[cmd][0], ['%s: %d' % (proto[cmd][1],d),
                '%s: %d' % (proto[cmd][2],d), '%d' % d]])     
            self.bitcount = self.databyte = 0
            self.state = state
            return
        self.putx([proto[cmd][0], ['%s[%d:%d]: %02X' % (proto[cmd][1],key1,key0,d),
                   '%s[%d:%d]: %02X' % (proto[cmd][2],key1,key0,d), '%02X' % d]])
        self.bitcount = self.databyte = 0
        if cmd == 'DATA':
            self.bits -= 8
        self.state = state
        

        

    def handle_CMD(self):       
        if self.bitcount == 0:
            self.DATAss = self.samplenum
            (sclk, sdata) = self.wait({0: 'f'})
            if sdata :
                self.wait({0 : 'r'})
                self.cmdset('R0W','FIND DATA')
                return

        if self.bitcount == 1:            
            if self.sdata :
                self.extended = 0
            else :
                self.extended = 1    

        if self.bitcount == 2:        
            if not self.extended :
                if self.sdata :                
                    self.cmdset('RR','FIND ADDRESS')
                    return
                else :               
                    self.cmdset('RW','FIND ADDRESS')
                    return
            
        if self.bitcount == 3:         
            if not self.sdata :
                if self.extended :               
                    self.cmdset('ERR','FIND BTEY_COUNT')
                    return
                else :                
                    self.cmdset('ERW','FIND BTEY_COUNT')
                    return
            elif self.extended :  
                self.ss = self.DATAss            
                self.es = self.samplenum
                self.putx([proto['CMD_WARNINGS'][0], proto['CMD_WARNINGS'][1:]])
                self.init()
                return

        if self.bitcount == 4:          
            if self.sdata :              
                self.cmdset('ERRL','FIND BTEY_COUNT')
                return
            else :                
                self.cmdset('ERWL','FIND BTEY_COUNT')
                return

        if self.bitcount <4:
            while True :
                if self._display :
                    (sclk,self.sdata) = self.wait([{0: 'f'},{0: 'l', 1: 'e'}])
                    if (self.matched & (0b1 << 0)):
                        break
                    if (self.matched & (0b1 << 1)):
                        self.ss = self.samplenum
                        (sclk,self.sdata) = self.wait([{0: 'f'},{0: 'l', 1: 'e'}])
                        if (self.matched & (0b1 << 0)):
                            self.es = self.samplenum
                            self.putx([proto['IJE'][0], proto['IJE'][1:]])
                            break
                        if (self.matched & (0b1 << 1)):
                            self.es = self.samplenum
                            self.putx([proto['IJE'][0], proto['IJE'][1:]])
                else :
                    (sclk,self.sdata) = self.wait({0: 'f'})
                    break
            self.wait({0 : 'r'})
            self.bitcount += 1

    def cmdset(self,cmd,state):
        self.ss = self.DATAss
        self.es = self.samplenum
        self.putx([proto[cmd][0], proto[cmd][1:]])
        self.state = state
        self.bitcount = 0
        self.extended = -1
        self.cmdkey = cmd

    def initBP(self,sclk,sdata,state,key):
        self.handle_BP('BP',state)
        self.state = state  
        if key :
            self.init()
           
    def init(self):
        self.cmdkey = 'NULL'
        self.ADDcount = 0
        self.Pcount = 0
        self.BPcount = 0
        self.BC = 0
        self.bitcount = 0
        self.Pes = 0
        self.state = 'FIND SSC'


        

    def decode(self):
        while True:
            if self.state == 'FIND SSC':
                self.wait({0: 'l', 1: 'r'})
                self.BPss = self.samplenum   
                self.wait([{0: 'h'},{0: 'l', 1: 'f'}])
                if (self.matched & (0b1 << 0)):
                    continue
                if (self.matched & (0b1 << 1)):
                    self.wait([{0: 'l', 1: 'e'},{0: 'r'}])
                    if (self.matched & (0b1 << 0)):
                        continue
                    if (self.matched & (0b1 << 1)):
                        self.ss,self.es = self.BPss,self.samplenum
                        self.putx([proto['SSC'][0], proto['SSC'][1:]])
                        self.state = 'FIND SLAVE ADDRESS'
                    
            elif self.state == 'FIND SLAVE ADDRESS':
                self.handle('SA','FIND COMMAND',3,0)
            
            elif self.state == 'FIND COMMAND':
                self.handle_CMD()
                
            elif  self.state == 'FIND BTEY_COUNT':
                if self.cmdkey == 'ERW' or self.cmdkey == 'ERR' :                   
                    self.handle('BC','FIND PARITY',3,0)
                else :                   
                    self.handle('BC','FIND PARITY',2,0)
                self.bits = self.BC*8

            elif self.state == 'FIND ADDRESS':
                if self.cmdkey == 'RW' or self.cmdkey == 'RR' :                  
                    self.handle('ADDRESS','FIND PARITY',4,0)
                elif self.cmdkey == 'ERR' or self.cmdkey == 'ERW' : 
                    self.handle('ADDRESS','FIND PARITY',7,0)
                else : 
                    if self.Pcount == 1  :                
                        self.handle('ADDRESS','FIND PARITY',15,8)
                    else :
                        self.handle('ADDRESS','FIND PARITY',7,0)

            elif self.state == 'FIND DATA':
                if self.cmdkey == 'R0W' :                   
                    self.handle('DATA','FIND PARITY',6,0)
                elif self.cmdkey == 'RW' or self.cmdkey == 'RR' :                  
                    self.handle('DATA','FIND PARITY',7,0)
                else :                   
                    self.handle('DATA','FIND PARITY',self.bits-1,self.bits-8)

            elif self.state == 'FIND PARITY':
                self.Pcount += 1
                self.handle('P','NULL',0,0)
                if self.cmdkey == 'R0W' :
                    self.state = 'FIND BUS_PARK'

                elif self.cmdkey == 'ERW' :
                    if self.Pcount == 1 :
                        self.ADDcount,self.state = 1,'FIND ADDRESS'  
                    elif self.Pcount == 2 :
                        self.state = 'FIND DATA'
                    elif self.Pcount == self.BC + 2 :
                        self.state = 'FIND BUS_PARK'
                        continue
                    elif self.Pcount > 2 :
                        self.state = 'FIND DATA'

                elif self.cmdkey == 'ERR' :
                    if self.Pcount == 1 :
                        self.ADDcount,self.state = 1,'FIND ADDRESS' 
                    elif self.Pcount == 2 :
                        self.BPcount,self.state = 1,'FIND BUS_PARK'
                    elif self.Pcount == self.BC + 2 :
                        self.BPcount =2
                        self.state = 'FIND BUS_PARK'
                        continue
                    elif self.Pcount > 2 :
                        self.state = 'FIND DATA'

                elif self.cmdkey == 'ERWL' :
                    if self.Pcount == 1 :
                        self.ADDcount,self.state = 2,'FIND ADDRESS' 
                    elif self.Pcount == 2 :    
                        self.ADDcount,self.state = 1,'FIND ADDRESS' 
                    elif self.Pcount == 3 :
                        self.state = 'FIND DATA'
                    elif self.Pcount == self.BC + 3 :
                        self.state = 'FIND BUS_PARK'
                        continue
                    elif self.Pcount > 3 :
                        self.state = 'FIND DATA'
                    
                elif self.cmdkey == 'ERRL' :
                    if self.Pcount == 1 :
                        self.ADDcount,self.state = 2,'FIND ADDRESS'
                    elif self.Pcount == 2 :
                        self.ADDcount,self.state = 1,'FIND ADDRESS'
                    elif self.Pcount == 3 :
                        self.BPcount,self.state = 1,'FIND BUS_PARK'
                    elif self.Pcount == self.BC + 3 :
                        self.BPcount =2
                        self.state = 'FIND BUS_PARK'
                        continue
                    elif self.Pcount > 3 :
                        self.state = 'FIND DATA'

                elif self.cmdkey == 'RW' :
                    if self.Pcount == 1 :    
                        self.state = 'FIND DATA'
                    elif self.Pcount == 2 :   
                        self.BPcount,self.state = 1,'FIND BUS_PARK'

                elif self.cmdkey == 'RR' :
                    self.state = 'FIND BUS_PARK'
                    self.BPcount = 1 if (self.Pcount == 1) else 2 

            elif self.state == 'FIND BUS_PARK':
                (sclk, sdata) = self.wait({0: 'l', 1: 'l'})
                self.ss = self.samplenum   
                if self.cmdkey == 'ERR' or self.cmdkey == 'ERRL' or self.cmdkey == 'RR':
                    key = 0 if (self.BPcount == 1) else 1 
                    self.initBP(sclk,sdata,'FIND DATA',key)
                else :
                    self.initBP(sclk,sdata,'FIND SSC',1)

            


                
            
        