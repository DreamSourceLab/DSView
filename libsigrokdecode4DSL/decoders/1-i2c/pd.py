##
## This file is part of the libsigrokdecode project.
##
## Copyright (C) 2010-2014 Uwe Hermann <uwe@hermann-uwe.de>
## Copyright (C) 2016 DreamSourceLab <support@dreamsourcelab.com>
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

# TODO: Look into arbitration, collision detection, clock synchronisation, etc.
# TODO: Implement support for 10bit slave addresses.
# TODO: Implement support for inverting SDA/SCL levels (0->1 and 1->0).
# TODO: Implement support for detecting various bus errors.

import sigrokdecode as srd

'''
OUTPUT_PYTHON format:

Packet:
[<ptype>, <pdata>]

<ptype>:
 - 'START' (START condition)
 - 'START REPEAT' (Repeated START condition)
 - 'ADDRESS READ' (Slave address, read)
 - 'ADDRESS WRITE' (Slave address, write)
 - 'DATA READ' (Data, read)
 - 'DATA WRITE' (Data, write)
 - 'STOP' (STOP condition)
 - 'ACK' (ACK bit)
 - 'NACK' (NACK bit)
 - 'BITS' (<pdata>: list of data/address bits and their ss/es numbers)

<pdata> is the data or address byte associated with the 'ADDRESS*' and 'DATA*'
command. Slave addresses do not include bit 0 (the READ/WRITE indication bit).
For example, a slave address field could be 0x51 (instead of 0xa2).
For 'START', 'START REPEAT', 'STOP', 'ACK', and 'NACK' <pdata> is None.
'''

# CMD: [annotation-type-index, long annotation, short annotation]
proto = {
    'START':           [0, 'Start',         'S'],
    'START REPEAT':    [1, 'Start repeat',  'Sr'],
    'STOP':            [2, 'Stop',          'P'],
    'ACK':             [3, 'ACK',           'A'],
    'NACK':            [4, 'NACK',          'N'],
    'READ':            [5, 'Read',          'R'],
    'WRITE':           [6, 'Write',         'W'],
    'BIT':             [7, 'Bit',           'B'],
    'ADDRESS READ':    [8, 'Address read',  'AR'],
    'ADDRESS WRITE':   [9, 'Address write', 'AW'],
    'DATA READ':       [10, 'Data read',     'DR'],
    'DATA WRITE':      [11, 'Data write',    'DW'],
}

class SamplerateError(Exception):
    pass

class Decoder(srd.Decoder):
    api_version = 2
    id = '1:i2c'
    name = '1:I²C'
    longname = 'Inter-Integrated Circuit'
    desc = 'Two-wire, multi-master, serial bus.'
    license = 'gplv2+'
    inputs = ['logic']
    outputs = ['i2c']
    channels = (
        {'id': 'scl', 'type': 8, 'name': 'SCL', 'desc': 'Serial clock line'},
        {'id': 'sda', 'type': 108, 'name': 'SDA', 'desc': 'Serial data line'},
    )
    options = (
        {'id': 'address_format', 'desc': 'Displayed slave address format',
            'default': 'shifted', 'values': ('shifted', 'unshifted')},
    )
    annotations = (
        ('7', 'start', 'Start condition'),
        ('6', 'repeat-start', 'Repeat start condition'),
        ('1', 'stop', 'Stop condition'),
        ('5', 'ack', 'ACK'),
        ('0', 'nack', 'NACK'),
        ('12', 'read', 'Read'),
        ('11', 'write', 'Write'),
        ('208', 'bit', 'Data/address bit'),
        ('112', 'address-read', 'Address read'),
        ('111', 'address-write', 'Address write'),
        ('110', 'data-read', 'Data read'),
        ('109', 'data-write', 'Data write'),
        ('1000', 'warnings', 'Human-readable warnings'),
    )
    annotation_rows = (
        ('bits', 'Bits', (7,)),
        ('addr-data', 'Address/Data', (0, 1, 2, 3, 4, 5, 6, 8, 9, 10, 11)),
        ('warnings', 'Warnings', (12,)),
    )
    binary = (
        ('address-read', 'Address read'),
        ('address-write', 'Address write'),
        ('data-read', 'Data read'),
        ('data-write', 'Data write'),
    )

    def __init__(self):
        self.samplerate = None
        self.ss = self.es = self.ss_byte = -1
        self.samplenum = None
        self.bitcount = 0
        self.databyte = 0
        self.wr = -1
        self.is_repeat_start = 0
        self.state = 'FIND START'
        self.oldscl = self.oldsda = -1
        self.pdu_start = None
        #self.pdu_bits = 0
        self.bits = []

    def metadata(self, key, value):
        if key == srd.SRD_CONF_SAMPLERATE:
            self.samplerate = value

    def start(self):
        self.out_python = self.register(srd.OUTPUT_PYTHON)
        self.out_ann = self.register(srd.OUTPUT_ANN)
        self.out_binary = self.register(srd.OUTPUT_BINARY)
        self.out_bitrate = self.register(srd.OUTPUT_META,
                meta=(int, 'Bitrate', 'Bitrate from Start bit to Stop bit'))
        if not self.samplerate:
            raise SamplerateError('Cannot decode without samplerate.')

    def putx(self, data):
        self.put(self.ss, self.es, self.out_ann, data)

    def putp(self, data):
        self.put(self.ss, self.es, self.out_python, data)

    #def putb(self, data):
    #    self.put(self.ss, self.es, self.out_binary, data)

    def found_start(self, scl, sda):
        self.ss, self.es = self.samplenum, self.samplenum
        self.pdu_start = self.samplenum
        #self.pdu_bits = 0
        cmd = 'START REPEAT' if (self.is_repeat_start == 1) else 'START'
        self.putp([cmd, None])
        self.putx([proto[cmd][0], proto[cmd][1:]])
        self.state = 'FIND ADDRESS'
        self.bitcount = self.databyte = 0
        self.is_repeat_start = 1
        self.wr = -1
        self.bits = []

    # Gather 7 bits of address, 1 bit of rd/wr,  plus the ACK/NACK bit.
    def found_address(self, scl, sda):
        # Address and data are transmitted MSB-first.
        self.databyte <<= 1
        self.databyte |= sda

        # Remember the start of the first data/address bit.
        if self.bitcount == 0:
            self.ss_byte = self.samplenum

        # Store individual bits and their start/end samplenumbers.
        # In the list, index 0 represents the MSB (I²C transmits MSB-first).
        self.bits.insert(0, [sda, self.samplenum, self.samplenum])
        if self.bitcount > 0:
            self.bits[1][2] = self.samplenum
        if self.bitcount == 7:
            self.bitwidth = self.bits[1][2] - self.bits[2][2]
            self.bits[0][2] += self.bitwidth

        # Return if we haven't collected all 8 + 1 bits, yet.
        if self.bitcount < 7:
            self.bitcount += 1
            return

        # The READ/WRITE bit is only in address bytes, not data bytes.
        self.wr = 0 if (self.databyte & 1) else 1
        if self.options['address_format'] == 'shifted':
            self.databyte = self.databyte >> 1
        cmd = 'ADDRESS WRITE' if self.wr else 'ADDRESS READ'
        #bin_class = 1 if self.wr else 0

        self.ss, self.es = self.ss_byte, self.samplenum

        self.putp(['BITS', self.bits])
        self.putp([cmd, self.databyte])

        #self.putb([bin_class, bytes([self.databyte])])

        for bit in reversed(self.bits):
            self.put(bit[1], bit[2], self.out_ann, [7, ['%d' % bit[0]]])

        self.putx([proto[cmd][0], ['%s: %02X' % (proto[cmd][1], self.databyte),
                   '%s: %02X' % (proto[cmd][2], self.databyte), '%02X' % self.databyte]])

        cmd = 'WRITE' if self.wr else 'READ'
        self.ss, self.es = self.samplenum, self.samplenum + self.bitwidth
        w = ['Write', 'Wr', 'W'] if self.wr else ['Read', 'Rd', 'R']
        self.putx([proto[cmd][0], w])

        # Done with this packet.
        self.bitcount = self.databyte = 0
        self.bits = []
        self.state = 'FIND ACK'

    # Gather 8 bits of data plus the ACK/NACK bit.
    def found_data(self, scl, sda):
        # Address and data are transmitted MSB-first.
        self.databyte <<= 1
        self.databyte |= sda

        # Remember the start of the first data/address bit.
        if self.bitcount == 0:
            self.ss_byte = self.samplenum

        # Store individual bits and their start/end samplenumbers.
        # In the list, index 0 represents the MSB (I²C transmits MSB-first).
        self.bits.insert(0, [sda, self.samplenum, self.samplenum])
        if self.bitcount > 0:
            self.bits[1][2] = self.samplenum
        if self.bitcount == 7:
            self.bitwidth = self.bits[1][2] - self.bits[2][2]
            self.bits[0][2] += self.bitwidth

        # Return if we haven't collected all 8 + 1 bits, yet.
        if self.bitcount < 7:
            self.bitcount += 1
            return

        cmd = 'DATA WRITE' if self.wr else 'DATA READ'
        #bin_class = 3 if self.wr else 2

        self.ss, self.es = self.ss_byte, self.samplenum + self.bitwidth

        self.putp(['BITS', self.bits])
        self.putp([cmd, self.databyte])

        #self.putb([bin_class, bytes([self.databyte])])

        for bit in reversed(self.bits):
            self.put(bit[1], bit[2], self.out_ann, [7, ['%d' % bit[0]]])

        self.putx([proto[cmd][0], ['%s: %02X' % (proto[cmd][1], self.databyte),
                   '%s: %02X' % (proto[cmd][2], self.databyte), '%02X' % self.databyte]])

        # Done with this packet.
        self.bitcount = self.databyte = 0
        self.bits = []
        self.state = 'FIND ACK'

    def get_ack(self, scl, sda):
        self.ss, self.es = self.samplenum, self.samplenum + self.bitwidth
        cmd = 'NACK' if (sda == 1) else 'ACK'
        self.putp([cmd, None])
        self.putx([proto[cmd][0], proto[cmd][1:]])
        # There could be multiple data bytes in a row, so either find
        # another data byte or a STOP condition next.
        self.state = 'FIND DATA'

    def found_stop(self, scl, sda):
        # Meta bitrate
        #elapsed = 1 / float(self.samplerate) * (self.samplenum - self.pdu_start + 1)
        #bitrate = int(1 / elapsed * self.pdu_bits)
        #self.put(self.ss_byte, self.samplenum, self.out_bitrate, bitrate)

        cmd = 'STOP'
        self.ss, self.es = self.samplenum, self.samplenum
        self.putp([cmd, None])
        self.putx([proto[cmd][0], proto[cmd][1:]])
        self.state = 'FIND START'
        self.is_repeat_start = 0
        self.wr = -1
        self.bits = []

    def decode(self, ss, es, logic):
        for (self.samplenum, pins) in logic:

            (scl, sda) = pins
            #self.pdu_bits += 1
            logic.logic_mask = 0b11
            logic.cur_pos = self.samplenum
            logic.edge_index = -1

            # State machine.
            if self.state == 'FIND START':
                # START condition (S): SDA = falling, SCL = high
                if (self.oldsda == 1 and sda == 0) and scl == 1:
                    self.found_start(scl, sda)
                    logic.exp_logic = 0b01
                    logic.logic_mask = 0b01
                    logic.edge_index = 0
                    scl = 0
                else:
                    logic.exp_logic = 0b01
                    logic.logic_mask = 0b11
                    logic.edge_index = 1
                    sda = 1
            elif self.state == 'FIND ADDRESS':
                # Data sampling of receiver: SCL = rising
                if self.oldscl == 0 and scl == 1:
                    self.found_address(scl, sda)
                # START condition (S): SDA = falling, SCL = high
                elif (self.oldsda == 1 and sda == 0) and scl == 1:
                    self.found_start(scl, sda)
                # STOP condition (P): SDA = rising, SCL = high
                elif (self.oldsda == 0 and sda == 1) and scl == 1:
                    self.found_stop(scl, sda)
            elif self.state == 'FIND DATA':
                # Data sampling of receiver: SCL = rising
                if self.oldscl == 0 and scl == 1:
                    self.found_data(scl, sda)
                # START condition (S): SDA = falling, SCL = high
                elif (self.oldsda == 1 and sda == 0) and scl == 1:
                    self.found_start(scl, sda)
                # STOP condition (P): SDA = rising, SCL = high
                elif (self.oldsda == 0 and sda == 1) and scl == 1:
                    self.found_stop(scl, sda)
            elif self.state == 'FIND ACK':
                # Data sampling of receiver: SCL = rising
                if self.oldscl == 0 and scl == 1:
                    self.get_ack(scl, sda)
                logic.exp_logic = 0b01
                logic.logic_mask = 0b01
                logic.edge_index = 0
                scl = 0

            # Save current SDA/SCL values for the next round.
            self.oldscl, self.oldsda = scl, sda
