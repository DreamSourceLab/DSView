##
## This file is part of the libsigrokdecode project.
##
## Copyright (C) 2011 Gareth McMullin <gareth@blacksphere.co.nz>
## Copyright (C) 2012-2014 Uwe Hermann <uwe@hermann-uwe.de>
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

import sigrokdecode as srd
from collections import namedtuple
from math import floor, ceil
from enum import Enum
Data = namedtuple('Data', ['ss', 'es', 'val'])

##
## 2024/4/8 DreamSourceLab : add flash module
## 2024/7/5 DreamSourceLab : qpp text update
##

# Key: (CPOL, CPHA). Value: SPI mode.
# Clock polarity (CPOL) = 0/1: Clock is low/high when inactive.
# Clock phase (CPHA) = 0/1: Data is valid on the leading/trailing clock edge.
spi_mode = {
    (0, 0): 0, # Mode 0
    (0, 1): 1, # Mode 1
    (1, 0): 2, # Mode 2
    (1, 1): 3, # Mode 3
}

class process_enum(Enum):
    COMMAND = 0
    WRITE_BYTE = 1
    WRITE_BYTE_CONTINUOUS = 2
    READ_BYTE = 3
    READ_BYTE_CONTINUOUS = 4
    CONTINUOUS_READ_MODE_BITS = 5
    ADDRESS_BY_MODE = 6
    ADDRESS_24BIT = 7
    ADDRESS_32BIT = 8
    DUMMY_BY_MODE = 9
    DUMMY_8BIT = 10
    DUMMY_32BIT = 11
    DUMMY_40BIT = 12
    
process_text = {
    process_enum.COMMAND : ['Command' , 'CMD'] , 
    process_enum.WRITE_BYTE : ['Write Data' , 'WD'] , 
    process_enum.WRITE_BYTE_CONTINUOUS : ['Write Data' , 'WD'] , 
    process_enum.READ_BYTE : ['Read Data' , 'RD'] , 
    process_enum.READ_BYTE_CONTINUOUS : ['Read Data' , 'RD'] , 
    process_enum.ADDRESS_24BIT : ['24-Bit Address' , 'AD'] , 
    process_enum.ADDRESS_32BIT : ['32-Bit Address' , 'AD'] , 
    process_enum.CONTINUOUS_READ_MODE_BITS : ['Continuous Read Mode bits' , 'M'] , 
}

class process_mode(Enum):
    SINGLE = 0 
    DUAL = 1
    QUAD = 2

class process_info:
    def __init__(self, enum, mode):
        self.enum = enum
        self.mode = mode
    
    def __eq__(self, other):
        if isinstance(other, self.__class__):
            return self.enum == other.enum and self.mode == other.mode
        return False

#read adress（address mode、24BIT,32BIT）
READ_ADDRESS = process_info(process_enum.ADDRESS_BY_MODE , process_mode.SINGLE)
READ_3B_ADDRESS = process_info(process_enum.ADDRESS_24BIT , process_mode.SINGLE)
READ_4B_ADDRESS = process_info(process_enum.ADDRESS_32BIT , process_mode.SINGLE)

#read DUAL Adress
READ_ADDRESS_DUAL = process_info(process_enum.ADDRESS_BY_MODE , process_mode.DUAL)
READ_3B_ADDRESS_DUAL = process_info(process_enum.ADDRESS_24BIT , process_mode.DUAL)
READ_4B_ADDRESS_DUAL = process_info(process_enum.ADDRESS_32BIT , process_mode.DUAL)

#read QUAD Adress
READ_ADDRESS_QUAD = process_info(process_enum.ADDRESS_BY_MODE , process_mode.QUAD)
READ_3B_ADDRESS_QUAD = process_info(process_enum.ADDRESS_24BIT , process_mode.DUAL)
READ_4B_ADDRESS_QUAD = process_info(process_enum.ADDRESS_32BIT , process_mode.QUAD)

#Read Byte（IO1, DUAL,QUAD）
READ_BYTE_SINGLE = process_info(process_enum.READ_BYTE , process_mode.SINGLE)
READ_BYTE_DUAL = process_info(process_enum.READ_BYTE , process_mode.DUAL)
READ_BYTE_QUAD = process_info(process_enum.READ_BYTE , process_mode.QUAD)

READ_BYTE_SINGLE_CONTINUOUS = process_info(process_enum.READ_BYTE_CONTINUOUS , process_mode.SINGLE)
READ_BYTE_DUAL_CONTINUOUS = process_info(process_enum.READ_BYTE_CONTINUOUS , process_mode.DUAL)
READ_BYTE_QUAD_CONTINUOUS = process_info(process_enum.READ_BYTE_CONTINUOUS , process_mode.QUAD)

#read m7-m0(DUAL,QUAD)
READ_MODE_BITS_DUAL = process_info(process_enum.CONTINUOUS_READ_MODE_BITS , process_mode.DUAL)
READ_MODE_BITS_QUAD = process_info(process_enum.CONTINUOUS_READ_MODE_BITS , process_mode.QUAD)

#write (io0)
WRITE_BYTE_SINGLE = process_info(process_enum.WRITE_BYTE , process_mode.SINGLE)
WRITE_BYTE_QUAD_CONTINUOUS = process_info(process_enum.WRITE_BYTE_CONTINUOUS , process_mode.QUAD)

#DUMMY
DUMMY_CYCLE = process_info(process_enum.DUMMY_BY_MODE , process_mode.SINGLE)
DUMMY_CYCLE_8BIT = process_info(process_enum.DUMMY_8BIT , process_mode.SINGLE)
DUMMY_CYCLE_8BIT_QUAD = process_info(process_enum.DUMMY_8BIT , process_mode.QUAD)

#Command list（command、name、abb、data after command）
command = {
   0x06 : ['Write Enable' , 'WREN' , []],
   0x04 : ['Write Disable' , 'WRDI' , []],
   0x50 : ['Write Enable for Volatile Status Register' , 'WRENFVSR' , []],
   0x05 : ['Read Status Register' , 'RDSR' , [READ_BYTE_SINGLE]],
   0x35 : ['Read Status Register' , 'RDSR' , [READ_BYTE_SINGLE]],
   0x15 : ['Read Status Register' , 'RDSR' , [READ_BYTE_SINGLE]],
   0x01 : ['Write Status Register' , 'WRSR' , [WRITE_BYTE_SINGLE]],
   0x31 : ['Write Status Register' , 'WRSR' , [WRITE_BYTE_SINGLE]],
   0x11 : ['Write Status Register' , 'WRSR' , [WRITE_BYTE_SINGLE]],
   0xC8 : ['Read Extended Register' , 'RER' , [READ_BYTE_SINGLE]],
   0xC5 : ['Write Extended Register' , 'WER' , [WRITE_BYTE_SINGLE]],
   0x03 : ['Read Data Bytes' , 'READ' , [READ_ADDRESS , READ_BYTE_SINGLE_CONTINUOUS]],
   0x13 : ['Read Data Bytes' , '4READ' , [READ_4B_ADDRESS , READ_BYTE_SINGLE_CONTINUOUS]],
   0x0B : ['Read Data Bytes at Higher Speed' , 'Fast Read' , [READ_ADDRESS , DUMMY_CYCLE_8BIT , READ_BYTE_SINGLE_CONTINUOUS]],
   0x0C : ['Read Data Bytes at Higher Speed' , '4Fast Read', [READ_4B_ADDRESS , DUMMY_CYCLE_8BIT , READ_BYTE_SINGLE_CONTINUOUS]],
   0x3B : ['Dual Output Fast Read' , 'DOFR' , [READ_ADDRESS ,DUMMY_CYCLE_8BIT , READ_BYTE_DUAL_CONTINUOUS]],
   0x3C : ['Dual Output Fast Read' , '4DOFR' , [READ_4B_ADDRESS ,DUMMY_CYCLE_8BIT , READ_BYTE_DUAL_CONTINUOUS]],
   0x6B : ['Quad Output Fast Read' , 'QOFR' , [READ_ADDRESS ,DUMMY_CYCLE_8BIT , READ_BYTE_QUAD_CONTINUOUS]],
   0x6C : ['Quad Output Fast Read' , '4QOFR' , [READ_4B_ADDRESS ,DUMMY_CYCLE_8BIT , READ_BYTE_QUAD_CONTINUOUS]],
   0xBB : ['Dual I/O Fast Read' , 'DIOFR' , [READ_ADDRESS_DUAL ,READ_MODE_BITS_DUAL , READ_BYTE_DUAL_CONTINUOUS]], 
   0xBC : ['Dual I/O Fast Read' , '4DIOFR' , [READ_4B_ADDRESS_DUAL , READ_MODE_BITS_DUAL , READ_BYTE_DUAL_CONTINUOUS]],
   0xEB : ['Quad I/O Fast Read' , 'QIOFR' , [READ_ADDRESS_QUAD , READ_MODE_BITS_QUAD , DUMMY_CYCLE_8BIT_QUAD , DUMMY_CYCLE_8BIT_QUAD , READ_BYTE_QUAD_CONTINUOUS]], 
   0xEC : ['Quad I/O Fast Read' , '4QIOFR' , [READ_4B_ADDRESS_QUAD , READ_MODE_BITS_QUAD , DUMMY_CYCLE_8BIT_QUAD , DUMMY_CYCLE_8BIT_QUAD , READ_BYTE_QUAD_CONTINUOUS]],
   0x77 : ['Set Burst with Wrap' , 'SBWW' , [DUMMY_CYCLE_8BIT_QUAD , DUMMY_CYCLE_8BIT_QUAD , DUMMY_CYCLE_8BIT_QUAD , READ_BYTE_QUAD_CONTINUOUS]],
   0x02 : ['Page Program' , 'PP' , [READ_ADDRESS] + [WRITE_BYTE_SINGLE] * 256],
   0x12 : ['Page Program' , '4PP' , [READ_4B_ADDRESS] + [WRITE_BYTE_SINGLE] * 256],
   0x32 : ['Quad Page Program' , 'QPP' , [READ_ADDRESS] + [WRITE_BYTE_QUAD_CONTINUOUS]],
   0x34 : ['Quad Page Program' , '4QPP' , [READ_4B_ADDRESS] + [WRITE_BYTE_QUAD_CONTINUOUS]],
   0x20 : ['Sector Erase' , 'SE' , [READ_ADDRESS]],
   0x21 : ['Sector Erase' , '4SE', [READ_4B_ADDRESS]],
   0x52 : ['32KB Block Erase' , 'BE32' , [READ_ADDRESS]],
   0x5C : ['32KB Block Erase' , '4BE32' , [READ_4B_ADDRESS]],
   0xD8 : ['64KB Block Erase' , 'BE64', [READ_ADDRESS]],
   0xDC : ['64KB Block Erase' , '4BE64', [READ_4B_ADDRESS]],
   0x60 : ['Chip Erase' , 'CE' , []],
   0xC7 : ['Chip Erase' , 'CE' , []],
   0xB9 : ['Deep Power-Down' , 'DP' , []],
   0x4B : ['Read Unique ID' , 'RUID', [DUMMY_CYCLE , READ_BYTE_SINGLE_CONTINUOUS]],
   0xB7 : ['Enter 4-Byte Address Mode' , 'EN4BADM' , []],
   0xE9 : ['Exit 4-Byte Address Mode' , 'EX4BADM' , []],
   0x30 : ['Clear SR Flags' , 'CLSRF' , []],
   0xAB : ['Release from Deep Power-Down and Read Device ID' , 'RDI' , []],
   0x90 : ['Read Manufacture ID/ Device ID' , 'REMS' , [READ_3B_ADDRESS , READ_BYTE_SINGLE , READ_BYTE_SINGLE]],
   0x92 : ['Read Manufacture ID/ Device ID Dual I/O' , 'REMS' , [READ_3B_ADDRESS_DUAL , READ_MODE_BITS_DUAL ,READ_BYTE_DUAL_CONTINUOUS]],
   0x94 : ['Read Manufacture ID/ Device ID Quad I/O' , 'REMS' , [READ_3B_ADDRESS_QUAD , READ_MODE_BITS_QUAD , DUMMY_CYCLE_8BIT_QUAD , DUMMY_CYCLE_8BIT_QUAD , READ_BYTE_QUAD_CONTINUOUS]],
   0x9F : ['Read Identification' , 'RDID', [READ_BYTE_SINGLE_CONTINUOUS]],
   0x75 : ['Program/Erase Suspend' , 'PES' , []],
   0x7A : ['Program/Erase Resume' , 'PER' , []],
   0x44 : ['Erase Security Registers' , 'ESR' , [READ_ADDRESS]],
   0x42 : ['Program Security Registers' , 'PSR' , [READ_ADDRESS] + [WRITE_BYTE_SINGLE] * 256],
   0x48 : ['Read Security Registers' , 'RSR' , [READ_ADDRESS , DUMMY_CYCLE_8BIT , READ_BYTE_SINGLE_CONTINUOUS]],
   0x66 : ['Enable Reset' , 'ENRE' , []],
   0x99 : ['Reset' , 'RE' , []],
   0x5A : ['Read Serial Flash Discoverable Parameter' , 'RSFDP', [READ_3B_ADDRESS , DUMMY_CYCLE_8BIT , READ_BYTE_SINGLE_CONTINUOUS]],
}

class ChannelError(Exception):
    pass

class Decoder(srd.Decoder):
    api_version = 3
    id = 'smart_qspi'
    name = 'Smart QSPI'
    longname = 'Quad Serial Peripheral Interface'
    desc = 'Full-duplex, synchronous, serial bus.compatible with Dual SPI and Quad SPI interfaces.'
    license = 'gplv2+'
    inputs = ['logic']
    outputs = ['spi']
    tags = ['Embedded/industrial']
    channels = (
        {'id': 'clk', 'type': 0, 'name': 'CLK', 'desc': 'Clock', 'idn':'dec_qspi_chan_clk'},
        {'id': 'io0', 'type': 109, 'name': 'IO0', 'desc': 'Data i/o 0', 'idn':'dec_qspi_chan_io0'},
    )
    optional_channels = (
        {'id': 'io1', 'type': 107, 'name': 'IO1', 'desc': 'Data i/o 1', 'idn':'dec_qspi_opt_chan_io1'},
        {'id': 'io2', 'type': 107, 'name': 'IO2', 'desc': 'Data i/o 2', 'idn':'dec_qspi_opt_chan_io2'},
        {'id': 'io3', 'type': 107, 'name': 'IO3', 'desc': 'Data i/o 3', 'idn':'dec_qspi_opt_chan_io3'},
        {'id': 'cs', 'type': -1, 'name': 'CS#', 'desc': 'Chip-select', 'idn':'dec_qspi_opt_chan_cs'},
    )
    options = (
        {'id': 'cs_polarity', 'desc': 'CS# polarity', 'default': 'active-low',
            'values': ('active-low', 'active-high'), 'idn':'dec_qspi_opt_cs_polarity'},
        {'id': 'cpol', 'desc': 'Clock polarity (CPOL)', 'default': 0,
            'values': (0, 1), 'idn':'dec_qspi_opt_cpol'},
        {'id': 'cpha', 'desc': 'Clock phase (CPHA)', 'default': 0,
            'values': (0, 1), 'idn':'dec_qspi_opt_cpha'},
        {'id': 'bitorder', 'desc': 'Bit order',
            'default': 'msb-first', 'values': ('msb-first', 'lsb-first'), 'idn':'dec_qspi_opt_bitorder'},
        {'id': 'ads', 'desc': 'Adress Mode', 'default': '24-Bit Address',
            'values': ('32-Bit Address', '24-Bit Address'), 'idn':'dec_qspi_adress_mode'},
        {'id': 'frame', 'desc': 'Frame Decoder', 'default': 'no',
            'values': ('yes', 'no'), 'idn':'dec_qspi_opt_frame'},
        {'id': 'twolinesmode', 'desc': 'TwoLinesMode', 'default':'spi',
            'values':('spi','dspi','qspi'), 'idn':'dec_qspi_two_lines_mode'},
        {'id': 'invalidlevel', 'desc': 'Keep high or low as invalid', 'default':'both',
            'values':('both','low','high'), 'idn':'dec_qspi_invalidlevel'},

    )
    annotations = (
        ('6', 'data-transfer', 'data transfer'),
        ('108', 'Quad data', 'Q-Data'),
        ('108', 'Dual data', 'D-Data'),
        ('106', 'd0', 'IO0 data'),
        ('106', 'd1', 'IO1 data'),
        ('106', 'd2', 'IO2 data'),
        ('106', 'd3', 'IO3 data'),
        ('1000', 'other', 'Human-readable warnings'),
    )
    annotation_rows = (
        ('data-transfer', 'data transfer', (0,)),
        ('Quad data', 'Q-Data', (1,)),
        ('Dual data', 'D-Data', (2,)),
        ('d0', 'D0', (3,)),
        ('d1', 'D1', (4,)),
        ('d2', 'D2', (5,)),
        ('d3', 'D3', (6,)),
        ('Other', 'Other',(7,)),
    )

    def __init__(self):
        self.reset()

    def reset(self):
        self.samplerate = None
        self.bitcount = 0
        self.data = 0
        self.bits = []
        self.io0bits=[]
        self.io1bits=[]
        self.io2bits=[]
        self.io3bits=[]
        self.io0bytes=[]
        self.io1bytes=[]
        self.io2bytes=[]
        self.io3bytes=[]
        self.io0data=0
        self.io1data=0
        self.io2data=0
        self.io3data=0

        self.command = 0

        self.state_count = 0
        self.ads = False
        self.diagram = []

        self.ss = 0
        self.count = 0
        self.bits_data = 0

        self.ss_block = -1
        self.samplenum = -1
        self.ss_transfer = -1
        self.cs_was_deasserted = False
        self.have_cs = self.have_io1 = self.have_io3 = None

    def start(self):
        self.out_python = self.register(srd.OUTPUT_PYTHON)
        self.out_ann = self.register(srd.OUTPUT_ANN)
        self.out_binary = self.register(srd.OUTPUT_BINARY)
        self.out_bitrate = self.register(srd.OUTPUT_META,
                meta=(int, 'Bitrate', 'Bitrate during transfers'))
        self.bw = (8 + 7) // 8
        self.spiModeSet=self.options['twolinesmode'].lower()
        self.invalidlevelSet = 0XFF if self.options['invalidlevel'] == 'high' else 0

        self.ads = False if self.options['ads'] == '24-Bit Address' else True

    def metadata(self, key, value):
       if key == srd.SRD_CONF_SAMPLERATE:
            self.samplerate = value
            self.bit_width = float(self.samplerate) / float(115200)

    def putw(self, data):
        self.put(self.ss_block, self.samplenum, self.out_ann, data)

    def putgse(self, ss, es, data):
        self.put(ss, es, self.out_ann, data)

    def putg(self, data):
        s, halfbit = self.samplenum, self.bit_width / 2.0
        self.put(s - floor(halfbit), s + ceil(halfbit), self.out_ann, data)  

    def puttext(self, ss, es, state, data):
        self.put(ss, es, self.out_ann, [0, ['%s : 0x%02x' % (process_text[state][0] , data),
            '%s : 0x%02x' % (process_text[state][1] , data),
            '0x%02X' % data]])

    def putdummy(self, ss, es):
        self.put(ss, es, self.out_ann, [0, ['Dummy Cycles','Dummy','D']])

    def putcmd(self, ss, es):
        self.put(ss, es, self.out_ann, [0, ['%s(0x%02x)' % (command[self.io0data][0] , self.io0data),
                                        '%s(0x%02x)' % (command[self.io0data][1] , self.io0data),
                                        '%s' % command[self.io0data][1]]])

    def checkinvalidlevel(self,data): # if data line is keep high or keep low,data is invalid then return true
        if self.options['invalidlevel']=='both' :
            return data==0xff or data ==0
        elif self.options['invalidlevel']=='high':
            return data==0xff
        else :
            return data==0


    def putdata(self, frame):
        data0 = self.io0data
        data1 = self.io1data
        data2 = self.io2data if self.have_io3 else None
        data3 = self.io3data if self.have_io3 else None

        ss,es = self.io0bits[-1][1], self.io0bits[0][2]
        if frame:
            if self.have_io3:
                self.io2bytes.append(Data(ss = ss, es = es, val = data2))
                self.io3bytes.append(Data(ss = ss, es = es, val = data3))
            if self.have_io1:
                self.io1bytes.append(Data(ss = ss, es = es, val = data1))
            self.io0bytes.append(Data(ss = ss, es = es, val = data0))

        bo = self.options['bitorder']

        if(((self.checkinvalidlevel(data2) or self.checkinvalidlevel(data3)) and self.have_io3) or (self.checkinvalidlevel(data1) and self.have_io1)):
            spiMode  = self.spiModeSet
        elif self.have_io3:
            spiMode='qspi'
        else :
            spiMode='spi'

        if spiMode == 'qspi' or spiMode == 'dspi':

            qdata = []
            ddata = []
            if self.have_io1 and self.have_io3:
                for i in range(4):
                    self.data = 0
                    for j in range(2):
                        self.data |= self.io3bits[-1 - i * 2 - j][0]
                        self.data = self.data << 1
                        self.data |= self.io2bits[-1 - i * 2 - j][0]
                        self.data = self.data << 1
                        self.data |= self.io1bits[-1 - i * 2 - j][0]
                        self.data = self.data << 1
                        self.data |= self.io0bits[-1 - i * 2 - j][0]
                        if j < 1:
                            self.data = self.data << 1
                    
                    qss,qes = self.io0bits[-1 - i * 2][1], self.io0bits[-2 - i * 2][2]

                    if not bo == 'msb-first':  
                        dataTemp = (0xF0 & self.data) >> 4|(0x0f & self.data) << 4
                        self.data = dataTemp

                    qdata.append([self.data, qss, qes])
                    self.put(qss, qes, self.out_ann, [1, ['@%02X' % self.data]])
            if self.have_io1:
                for i in range(2):
                    self.data = 0
                    for j in range(4):
                        self.data |= self.io1bits[-1 - i * 4 - j][0]
                        self.data = self.data << 1
                        self.data |= self.io0bits[-1 - i * 4 - j][0]
                        if j < 3:
                            self.data = self.data << 1
                    dss,des = self.io0bits[-1 - i * 4][1], self.io0bits[-4 - i * 4][2]
                    if not bo == 'msb-first':  
                        dataTemp = (0xF0 & self.data) >> 4|(0x0f & self.data) << 4
                        self.data = dataTemp
                    
                    ddata.append([self.data, dss, des])
                    self.put(dss, des, self.out_ann, [2, ['@%02X' % self.data]])
            
            if self.command == 0 and self.io0data in command:
                self.putcmd(ss, es)
                #update adress mode
                if self.io0data == 0xB7:
                    self.ads = True
                elif self.io0data == 0xE9:
                    self.ads = False

                if len(command[self.io0data][2]) != 0:
                    self.command = self.io0data
                    self.diagram = command[self.command][2]

            elif self.command != 0:
                cur_state = self.diagram[self.state_count]

                i, size = 0, 1
                if cur_state.mode == process_mode.DUAL:
                    size = 2
                elif cur_state.mode == process_mode.QUAD:
                    size = 4

                while i < size:
                    if size == 1:
                        if cur_state.enum == process_enum.READ_BYTE and self.have_io1:
                            origin_data = [self.io1data & 0xFF , ss, es]
                        else:
                            origin_data = [self.io0data & 0xFF , ss, es]
                    elif size == 2:
                        origin_data = ddata[i]
                        origin_data[0] = origin_data[0] & 0xFF
                    elif size == 4:
                        origin_data = qdata[i]
                        origin_data[0] = origin_data[0] & 0xFF
                    i = i + 1

                    if cur_state.enum  == process_enum.ADDRESS_BY_MODE or \
                        cur_state.enum  == process_enum.ADDRESS_24BIT or \
                        cur_state.enum  == process_enum.ADDRESS_32BIT or \
                        cur_state.enum  == process_enum.DUMMY_BY_MODE or \
                        cur_state.enum  == process_enum.DUMMY_8BIT:

                        if cur_state.enum == process_enum.ADDRESS_BY_MODE:
                            cur_state.enum = process_enum.ADDRESS_24BIT if self.ads == False else process_enum.ADDRESS_32BIT
                        elif cur_state.enum == process_enum.DUMMY_BY_MODE:
                            cur_state.enum = process_enum.DUMMY_32BIT if self.ads == False else process_enum.DUMMY_40BIT

                        if self.count == 0:
                            self.ss = ss
                        self.bits_data |= origin_data[0]
                        self.count = self.count + 1

                        if (cur_state.enum == process_enum.ADDRESS_24BIT and self.count == 3) or \
                            (cur_state.enum == process_enum.ADDRESS_32BIT and self.count == 4) or \
                            (cur_state.enum == process_enum.DUMMY_8BIT and self.count == 1) or \
                            (cur_state.enum == process_enum.DUMMY_32BIT and self.count == 4) or  \
                            (cur_state.enum == process_enum.DUMMY_40BIT and self.count == 5):
                            
                            if cur_state.enum == process_enum.ADDRESS_24BIT or \
                                cur_state.enum == process_enum.ADDRESS_32BIT:
                                self.puttext(self.ss , origin_data[2] , cur_state.enum , self.bits_data)
                            else:
                                self.putdummy(self.ss, origin_data[2])
                            
                            self.ss = 0
                            self.count = 0
                            self.bits_data = 0
                            self.state_count += 1

                            if self.state_count < len(self.diagram):
                                cur_state = self.diagram[self.state_count]
                        else:
                            self.bits_data = self.bits_data << 8

                    elif cur_state.enum == process_enum.WRITE_BYTE or \
                        cur_state.enum == process_enum.READ_BYTE or \
                        cur_state.enum == process_enum.READ_BYTE_CONTINUOUS or \
                        cur_state.enum == process_enum.WRITE_BYTE_CONTINUOUS or \
                        cur_state.enum == process_enum.CONTINUOUS_READ_MODE_BITS:

                        self.puttext(origin_data[1] , origin_data[2] , cur_state.enum , origin_data[0])
                        
                        if cur_state.enum != process_enum.READ_BYTE_CONTINUOUS and \
                        cur_state.enum != process_enum.WRITE_BYTE_CONTINUOUS:
                            self.state_count += 1

                            if self.state_count < len(self.diagram):
                                cur_state = self.diagram[self.state_count]

                    if self.state_count >= len(self.diagram):
                        self.command = 0
                        self.diagram = []
                        self.state_count = 0
                        break

    def reset_decoder_state(self):

        self.io0data = 0
        self.io1data = 0 if self.have_io1 else None
        self.io2data=0 if self.have_io3 else None
        self.io3data=0 if self.have_io3 else None

        self.io0bits=[]
        self.io1bits=[] if self.have_io1 else None
        self.io2bits=[] if self.have_io3 else None
        self.io3bits=[] if self.have_io3 else None
        self.bitcount = 0

    def cs_asserted(self, cs):
        active_low = (self.options['cs_polarity'] == 'active-low')
        return (cs == 0) if active_low else (cs == 1)

    def handle_bit(self, datapins, clk, cs, frame):
        # If this is the first bit of a dataword, save its sample number.
        if self.bitcount == 0:
            self.ss_block = self.samplenum
            self.cs_was_deasserted = \
                not self.cs_asserted(cs) if self.have_cs else False

        ws = 8
        bo = self.options['bitorder']

        if bo == 'msb-first':
            self.io0data |= datapins[3] << (ws - 1 - self.bitcount)
        else:
            self.io0data |= datapins[3] << self.bitcount
        if self.have_io1:
            if bo == 'msb-first':
                self.io1data |= datapins[2] << (ws - 1 - self.bitcount)
            else:
                self.io1data |= datapins[2] << self.bitcount

        if self.have_io3:
            if bo == 'msb-first':
                self.io2data |= datapins[1] << (ws - 1 - self.bitcount)
                self.io3data |= datapins[0] << (ws - 1 - self.bitcount)
            else:
                self.io2data |= datapins[1] << self.bitcount
                self.io3data |= datapins[0] << self.bitcount

        # Guesstimate the endsample for this bit (can be overridden below).
        es = self.samplenum
        if self.bitcount > 0:
            es += self.samplenum - self.io0bits[0][1]

        self.io0bits.insert(0, [datapins[3], self.samplenum, es])
        if self.have_io1:
            self.io1bits.insert(0, [datapins[2], self.samplenum, es])
        if self.have_io3:
            self.io2bits.insert(0, [datapins[1], self.samplenum, es])
            self.io3bits.insert(0, [datapins[0], self.samplenum, es])

        if self.bitcount > 0:
            self.io0bits[1][2] = self.samplenum
            if self.have_io1:
                self.io1bits[1][2] = self.samplenum
            if self.have_io3:
                self.io2bits[1][2] = self.samplenum
                self.io3bits[1][2] = self.samplenum

        self.bitcount += 1

        # Continue to receive if not enough bits were received, yet.
        if self.bitcount != ws: 
            return

        self.putdata(frame)
        if self.samplerate:
            elapsed = 1 / float(self.samplerate)
            elapsed *= (self.samplenum - self.ss_block + 1)
            bitrate = int(1 / elapsed * ws)
            self.put(self.ss_block, self.samplenum, self.out_bitrate, bitrate)

        if self.have_cs and self.cs_was_deasserted:
            self.putw([6, ['CS# was deasserted during this data word!']])

        self.reset_decoder_state()

    def find_clk_edge(self, datapins, clk, cs, first,frame):

        if self.have_cs and (first or (self.matched & (0b1 << self.have_cs))):
            # Send all CS# pin value changes.
            oldcs = None if first else 1 - cs
            self.put(self.samplenum, self.samplenum, self.out_python,
                ['CS-CHANGE', oldcs, cs])
            
            if frame:
                if self.cs_asserted(cs):
                    self.ss_transfer = self.samplenum
                    self.io0bytes = []
                    self.io1bytes = []
                    self.io2bytes = []
                    self.io3bytes = []

                elif self.ss_transfer != -1:
                    self.put(self.ss_transfer, self.samplenum, self.out_python,
                        ['TRANSFER', self.io0bytes, self.io1bytes, self.io2bytes,self.io3bytes])     
            # Reset decoder state when CS# changes (and the CS# pin is used).
            self.command = 0
            self.diagram = []
            self.state_count = 0

            self.ss = 0
            self.count = 0
            self.bits_data = 0

            self.reset_decoder_state()

        # We only care about samples if CS# is asserted.
        if self.have_cs and not self.cs_asserted(cs):
            return

        # Ignore sample if the clock pin hasn't changed.
        if first or not (self.matched & (0b1 << 0)):
            return

        # Found the correct clock edge, now get the SPI bit(s).
        self.handle_bit(datapins, clk, cs,frame)

    def decode(self):
        # The CLK & IO0 input is mandatory. Other signals are (individually)
        # optional. Tell stacked decoders when we don't have a CS# signal.
        if not self.has_channel(0):
            raise ChannelError('CLK pin required.')
        if not self.has_channel(1):
            raise ChannelError('io0 pin required.')

        self.have_io1 = self.has_channel(2)
        self.have_io3 = self.has_channel(3) & self.has_channel(4)
        self.have_cs = self.has_channel(5)

        if not self.have_cs:
            self.put(0, 0, self.out_python, ['CS-CHANGE', None, None])
        
        frame = self.options['frame'] == 'yes'

        # We want all CLK changes. We want all CS changes if CS is used.
        # Map 'have_cs' from boolean to an integer index. This simplifies
        # evaluation in other locations.
        # Sample data on rising/falling clock edge (depends on mode).
        mode = spi_mode[self.options['cpol'], self.options['cpha']]
        if mode == 0 or mode == 3:   # Sample on rising clock edge
            wait_cond = [{0: 'r'}]
        else: # Sample on falling clock edge
            wait_cond = [{0: 'f'}]

        if self.have_cs:
            self.have_cs = len(wait_cond)
            wait_cond.append({5: 'e'})

        # "Pixel compatibility" with the v2 implementation. Grab and
        # process the very first sample before checking for edges. The
        # previous implementation did this by seeding old values with
        # None, which led to an immediate "change" in comparison.
        (clk, d0, d1, d2, d3, cs) = self.wait({})
        d = (d3, d2, d1, d0)
        self.find_clk_edge(d, clk, cs, True ,frame)

        while True:
            (clk, d0, d1, d2, d3, cs) = self.wait(wait_cond)
            d = (d3, d2, d1, d0)
            self.find_clk_edge(d, clk, cs, False,frame)#

    
