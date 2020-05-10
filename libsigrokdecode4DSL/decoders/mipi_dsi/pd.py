##
## This file is part of the libsigrokdecode project.
## Author: Shiqiu Nie (369614718@qq.com)
## Version: 0.2
## Date: 2019-07-01
## History:
##  1. 2019-07-01 Create decoder
##  2. 2017-01-17 V0.1,Decode EscMode,BTA,Data,Stop,Idle
##  3. 2019-07-01 V0.2,Support for V3 decoder
##
## Copyright (C) 2010-2016 Uwe Hermann <uwe@hermann-uwe.de>
## Copyright (C) 2019 DreamSourceLab <support@dreamsourcelab.com>
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

# TODO: 

import sigrokdecode as srd

'''
OUTPUT_PYTHON format:

Packet:
[<ptype>, <pdata>]

<ptype>:
 

<pdata> is the data or address byte associated with the 'ADDRESS*' and 'DATA*'
command. 
'''

# CMD: [annotation-type-index, long annotation, short annotation]
proto = {
    'ESC Mode':           [0, 'Escape mode entry',                  'ESC'],
    'BTA':                [1, 'Bi-directional Data Lane Turnaround','BTA'],
    'DATA':               [2, 'Data',                              'Data'],
    'STOP':               [3, 'Stop',                                 'S'],
    'LPDT':               [4, 'LPDT Command',                      'LPDT'],
    'DI':                 [5, 'Data Identifier',                     'DI'],
    'ECC':                [6, 'Error Correction Code',              'ECC'],
    'WC':                 [7, 'Word count',                          'WC'],
    'CRC':                [8, 'CheckSUM',                           'CRC'],
    'ULPS':               [9, 'Ultra-Low Power State',             'ULPS'],
    'IDLE':               [10,'Idle',                              'Idle'],
}
# LP state
lp_state = ['LP-00','LP-01','LP-10','LP-11']

class Decoder(srd.Decoder):
    api_version = 3
    id = 'MIPI_DSI'
    name = 'MIPI_DSI'
    longname = 'MIPI Display Serial Interface'
    desc = 'MIPI Display Serial Interface low power communication'
    license = 'gplv2+'
    inputs = ['logic']
    outputs = ['mipi_dsi']
    tags = ['Embedded/industrial']
    channels = (
        {'id': 'D0N', 'type': 8, 'name': 'D0N', 'desc': 'LP data 0 neg'},
        {'id': 'D0P', 'type': 108, 'name': 'D0P', 'desc': 'LP data 0 pos'},
    )
    options = (

    )
    annotations = (
        ('111', 'LP-00', 'LP-00'),
        ('110', 'LP-01', 'LP-01'),
        ('109', 'LP-10', 'LP-10'),
        ('108', 'LP-11', 'LP-11'),
        ('7', 'EscapeMode', 'Escape mode'),
        ('6', 'BTA', 'Bi-directional Data Lane Turnaround'),
        ('112', 'LPDT', 'LPDT'),
        ('0', 'DI', 'Data identifier'),
        ('12', 'ECC', 'ECC'),
        ('11', 'WC', 'Word count'),
        ('107', 'CRC', 'CheckSUM'),
        ('5', 'Stop', 'Stop condition'),
        ('1', 'Idle', 'Idle'),
    )
    annotation_rows = (
        ('LPData', 'LPData', (0, 1, 2, 3,)),
        ('LP', 'LP', (4, 5, 6, 7, 8, 9, 10)),
    )
    binary = (
    )

    def __init__(self):
        self.reset()

    def reset(self):
        self.samplerate = None
        self.ss = self.es = self.ss_byte = -1
        self.bitcount = 0
        self.databyte = 0
        self.is_esc_bta = 0   # 0=ESC Mode 1=BTA Mode
        self.state = 'FIND START'
        self.pdu_start = None
        self.pdu_bits = 0
        self.bits = []
        self.findmode_state = 'Find Mode state0'

    def metadata(self, key, value):
        if key == srd.SRD_CONF_SAMPLERATE:
            self.samplerate = value

    def start(self):
        self.out_python = self.register(srd.OUTPUT_PYTHON)
        self.out_ann = self.register(srd.OUTPUT_ANN)
        self.out_binary = self.register(srd.OUTPUT_BINARY)
        self.out_bitrate = self.register(srd.OUTPUT_META,
                meta=(int, 'Bitrate', 'Bitrate from Start bit to Stop bit'))

    def putx(self, data):
        self.put(self.ss, self.es, self.out_ann, data)

    def putp(self, data):
        self.put(self.ss, self.es, self.out_python, data)

    def putb(self, data):
        self.put(self.ss, self.es, self.out_binary, data)

    def handle_start(self):
        self.ss, self.es = self.samplenum, self.samplenum
        self.pdu_start = self.samplenum
        self.pdu_bits = 0
        self.state = 'FIND MODE'
        self.bitcount = self.databyte = 0
        self.findmode_state = 'Find Mode state0'
        self.bits = []

    def handle_esc_bta(self, d0n, d0p):
        self.es = self.samplenum
        cmd = 'ESC Mode' if(d0n == 1) else 'BTA'
        self.putp([cmd, None])
        self.putx([proto[cmd][0], proto[cmd][1:]])
        self.bitcount = self.databyte = 0
        self.bits = []
        self.state = 'FIND DATA'
        self.ss = self.samplenum

    def handle_stop(self):
        cmd = 'STOP'
        self.es = self.samplenum
        self.putp([cmd, None])
        self.putx([proto[cmd][0], proto[cmd][1:]])
        self.state = 'FIND START'
        self.is_esc_bta = 0
        self.bits = []

    def handle_data(self, d0n, d0p):
        self.pdu_bits += 1

        self.databyte >>= 1
        if d0p:
            self.databyte |= 0x80

        if self.bitcount == 0:
            self.ss_byte = self.samplenum

        if self.bitcount < 7:
            self.bitcount += 1
            return

        d = self.databyte

        self.es = self.samplenum
        h = '0x%02X' % d
        cmd = 'DATA'
        self.putb([cmd, None])
        self.putx([proto[cmd][0], [h]])
        
        self.bitcount = self.databyte = 0
        self.bits = []
        self.state = 'FIND DATA'
        self.ss = self.samplenum

    def decode(self):
        while True:
            # State machine.
            if self.state == 'FIND START':
                # Wait for a START condition (S): D0P = high, D0N = falling.
                self.wait({0: 'f', 1: 'h'})
                self.handle_start()
            elif self.state == 'FIND MODE':
                if self.findmode_state == 'Find Mode state0':
                    self.wait({0: 'l', 1: 'l'})
                    self.findmode_state = 'Find Mode state1'
                elif self.findmode_state == 'Find Mode state1':
                    (d0n, d0p) = self.wait([{0: 'h', 1: 'l'}, {0: 'l', 1: 'h'}])
                    self.findmode_state = 'Find Mode state2'
                elif self.findmode_state == 'Find Mode state2':
                    self.wait({0: 'l', 1: 'l'})
                    self.handle_esc_bta(d0n, d0p)
                    #self.findmode_state = 'Find Mode state0'
            elif self.state == 'FIND DATA':
                (d0n, d0p) = self.wait([{0: 'h', 1: 'l'}, {0: 'l', 1: 'h'}])
                self.wait([{0: 'l', 1: 'l'}, {0: 'h', 1: 'h'}])
                
                if (self.matched & (0b1 << 0)):
                    self.handle_data(d0n, d0p)
                else :
                    self.handle_stop()
                
                
