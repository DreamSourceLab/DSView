##
## This file is part of the libsigrokdecode project.
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

# TODO: Look into arbitration, collision detection, clock synchronisation, etc.
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
    'ADDRESS READ':    [5, 'Address read',  'AR'],
    'ADDRESS WRITE':   [6, 'Address write', 'AW'],
    'DATA READ':       [7, 'Data read',     'DR'],
    'DATA WRITE':      [8, 'Data write',    'DW'],
}

class Decoder(srd.Decoder):
    api_version = 3
    id = '0:i2c'
    name = '0:I²C'
    longname = 'Inter-Integrated Circuit'
    desc = 'Two-wire, multi-master, serial bus.'
    license = 'gplv2+'
    inputs = ['logic']
    outputs = ['i2c']
    tags = ['Embedded/industrial']
    channels = (
        {'id': 'scl', 'type': 8, 'name': 'SCL', 'desc': 'Serial clock line', 'idn':'dec_0i2c_chan_scl'},
        {'id': 'sda', 'type': 108, 'name': 'SDA', 'desc': 'Serial data line', 'idn':'dec_0i2c_chan_sda'},
    )
    options = (
        {'id': 'address_format', 'desc': 'Displayed slave address format',
            'default': 'unshifted', 'values': ('shifted', 'unshifted'), 'idn':'dec_0i2c_opt_addr'},
    )
    annotations = (
        ('7', 'start', 'Start condition'),
        ('6', 'repeat-start', 'Repeat start condition'),
        ('1', 'stop', 'Stop condition'),
        ('5', 'ack', 'ACK'),
        ('0', 'nack', 'NACK'),
        ('112', 'address-read', 'Address read'),
        ('111', 'address-write', 'Address write'),
        ('110', 'data-read', 'Data read'),
        ('109', 'data-write', 'Data write'),
        ('1000', 'warnings', 'Human-readable warnings'),
    )
    annotation_rows = (
        ('addr-data', 'Address/Data', (0, 1, 2, 3, 4, 5, 6, 7, 8)),
        ('warnings', 'Warnings', (9,)),
    )

    def __init__(self):
        self.reset()

    def reset(self):
        self.samplerate = None
        self.ss = self.es = self.ss_byte = -1
        self.bitcount = 0
        self.databyte = 0
        self.wr = -1
        self.is_repeat_start = 0
        self.state = 'FIND START'
        self.pdu_start = None
        self.pdu_bits = 0
        self.bits = []

    def metadata(self, key, value):
        if key == srd.SRD_CONF_SAMPLERATE:
            self.samplerate = value

    def start(self):
        self.out_ann = self.register(srd.OUTPUT_ANN)

    def putx(self, data):
        self.put(self.ss, self.es, self.out_ann, data)

    def handle_start(self):
        self.ss, self.es = self.samplenum, self.samplenum
        self.pdu_start = self.samplenum
        self.pdu_bits = 0
        cmd = 'START REPEAT' if (self.is_repeat_start == 1) else 'START'
        self.putx([proto[cmd][0], proto[cmd][1:]])
        self.state = 'FIND ADDRESS'
        self.bitcount = self.databyte = 0
        self.is_repeat_start = 1
        self.wr = -1
        self.bits = []

    # Gather 8 bits of data plus the ACK/NACK bit.
    def handle_address_or_data(self, scl, sda):
        self.pdu_bits += 1

        # Address and data are transmitted MSB-first.
        self.databyte <<= 1
        self.databyte |= sda

        # Remember the start of the first data/address bit.
        if self.bitcount == 0:
            self.ss_byte = self.samplenum

        # Store individual bits and their start/end samplenumbers.
        # In the list, index 0 represents the LSB (I²C transmits MSB-first).
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

        d = self.databyte
        if self.state == 'FIND ADDRESS':
            # The READ/WRITE bit is only in address bytes, not data bytes.
            self.wr = 0 if (self.databyte & 1) else 1
            if self.options['address_format'] == 'shifted':
                d = d >> 1

        bin_class = -1
        if self.state == 'FIND ADDRESS' and self.wr == 1:
            cmd = 'ADDRESS WRITE'
            bin_class = 1
        elif self.state == 'FIND ADDRESS' and self.wr == 0:
            cmd = 'ADDRESS READ'
            bin_class = 0
        elif self.state == 'FIND DATA' and self.wr == 1:
            cmd = 'DATA WRITE'
            bin_class = 3
        elif self.state == 'FIND DATA' and self.wr == 0:
            cmd = 'DATA READ'
            bin_class = 2

        self.ss, self.es = self.ss_byte, self.samplenum + self.bitwidth

        if cmd.startswith('ADDRESS'):
            self.ss, self.es = self.samplenum, self.samplenum + self.bitwidth
            w = ['Write', 'Wr', 'W'] if self.wr else ['Read', 'Rd', 'R']
            self.putx([proto[cmd][0], w])
            self.ss, self.es = self.ss_byte, self.samplenum

        self.putx([proto[cmd][0], ['%s: {$}' % proto[cmd][1], '%s: {$}' % proto[cmd][2], '{$}', d]])

        # Done with this packet.
        self.bitcount = self.databyte = 0
        self.bits = []
        self.state = 'FIND ACK'

    def get_ack(self, scl, sda):
        self.ss, self.es = self.samplenum, self.samplenum + self.bitwidth
        cmd = 'NACK' if (sda == 1) else 'ACK'
        self.putx([proto[cmd][0], proto[cmd][1:]])
        # There could be multiple data bytes in a row, so either find
        # another data byte or a STOP condition next.
        self.state = 'FIND DATA'

    def handle_stop(self):
        cmd = 'STOP'
        self.ss, self.es = self.samplenum, self.samplenum
        self.putx([proto[cmd][0], proto[cmd][1:]])
        self.state = 'FIND START'
        self.is_repeat_start = 0
        self.wr = -1
        self.bits = []

    def decode(self):
        while True:
            # State machine.
            if self.state == 'FIND START':
                # Wait for a START condition (S): SCL = high, SDA = falling.
                self.wait({0: 'h', 1: 'f'})
                self.handle_start()
            elif self.state == 'FIND ADDRESS':
                # Wait for any of the following conditions (or combinations):
                #  a) Data sampling of receiver: SCL = rising, and/or
                #  b) START condition (S): SCL = high, SDA = falling, and/or
                #  c) STOP condition (P): SCL = high, SDA = rising
                (scl, sda) = self.wait([{0: 'r'}, {0: 'h', 1: 'f'}, {0: 'h', 1: 'r'}])

                # Check which of the condition(s) matched and handle them.
                if (self.matched & (0b1 << 0)):
                    self.handle_address_or_data(scl, sda)
                elif (self.matched & (0b1 << 1)):
                    self.handle_start()
                elif (self.matched & (0b1 << 2)):
                    self.handle_stop()
            elif self.state == 'FIND DATA':
                # Wait for any of the following conditions (or combinations):
                #  a) Data sampling of receiver: SCL = rising, and/or
                #  b) START condition (S): SCL = high, SDA = falling, and/or
                #  c) STOP condition (P): SCL = high, SDA = rising
                (scl, sda) = self.wait([{0: 'r'}, {0: 'h', 1: 'f'}, {0: 'h', 1: 'r'}])

                # Check which of the condition(s) matched and handle them.
                if (self.matched & (0b1 << 0)):
                    self.handle_address_or_data(scl, sda)
                elif (self.matched & (0b1 << 1)):
                    self.handle_start()
                elif (self.matched & (0b1 << 2)):
                    self.handle_stop()
            elif self.state == 'FIND ACK':
                # Wait for any of the following conditions (or combinations):
                #  a) a data/ack bit: SCL = rising.
                #  b) STOP condition (P): SCL = high, SDA = rising
                (scl, sda) = self.wait([{0: 'r'}, {0: 'h', 1: 'r'}])
                if (self.matched & (0b1 << 0)):
                    self.get_ack(scl, sda)
                elif (self.matched & (0b1 << 1)):
                    self.handle_stop()

