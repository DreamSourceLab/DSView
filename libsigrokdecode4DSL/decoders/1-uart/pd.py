##
## This file is part of the libsigrokdecode project.
##
## Copyright (C) 2011-2014 Uwe Hermann <uwe@hermann-uwe.de>
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

import sigrokdecode as srd
from math import floor, ceil

'''
OUTPUT_PYTHON format:

Packet:
[<ptype>, <pdata>]

This is the list of <ptype>s and their respective <pdata> values:
 - 'STARTBIT': The data is the (integer) value of the start bit (0/1).
 - 'DATA': This is always a tuple containing two items:
   - 1st item: the (integer) value of the UART data. Valid values
     range from 0 to 512 (as the data can be up to 9 bits in size).
   - 2nd item: the list of individual data bits and their ss/es numbers.
 - 'PARITYBIT': The data is the (integer) value of the parity bit (0/1).
 - 'STOPBIT': The data is the (integer) value of the stop bit (0 or 1).
 - 'INVALID STARTBIT': The data is the (integer) value of the start bit (0/1).
 - 'INVALID STOPBIT': The data is the (integer) value of the stop bit (0/1).
 - 'PARITY ERROR': The data is a tuple with two entries. The first one is
   the expected parity value, the second is the actual parity value.
 - TODO: Frame error?

'''

# Given a parity type to check (odd, even, zero, one), the value of the
# parity bit, the value of the data, and the length of the data (5-9 bits,
# usually 8 bits) return True if the parity is correct, False otherwise.
# 'none' is _not_ allowed as value for 'parity_type'.
def parity_ok(parity_type, parity_bit, data, num_data_bits):

    # Handle easy cases first (parity bit is always 1 or 0).
    if parity_type == 'zero':
        return parity_bit == 0
    elif parity_type == 'one':
        return parity_bit == 1

    # Count number of 1 (high) bits in the data (and the parity bit itself!).
    ones = bin(data).count('1') + parity_bit

    # Check for odd/even parity.
    if parity_type == 'odd':
        return (ones % 2) == 1
    elif parity_type == 'even':
        return (ones % 2) == 0

class SamplerateError(Exception):
    pass

class Decoder(srd.Decoder):
    api_version = 2
    id = '1:uart'
    name = '1:UART'
    longname = 'Universal Asynchronous Receiver/Transmitter'
    desc = 'Asynchronous, serial bus.'
    license = 'gplv2+'
    inputs = ['logic']
    outputs = ['uart']
    channels = (
        {'id': 'rxtx', 'type': 209, 'name': 'RX/TX', 'desc': 'UART transceive line'},
    )
    options = (
        {'id': 'baudrate', 'desc': 'Baud rate', 'default': 9600},
        {'id': 'num_data_bits', 'desc': 'Data bits', 'default': 8,
            'values': (5, 6, 7, 8, 9)},
        {'id': 'parity_type', 'desc': 'Parity type', 'default': 'none',
            'values': ('none', 'odd', 'even', 'zero', 'one')},
        {'id': 'parity_check', 'desc': 'Check parity?', 'default': 'yes',
            'values': ('yes', 'no')},
        {'id': 'num_stop_bits', 'desc': 'Stop bits', 'default': 1.0,
            'values': (0.0, 0.5, 1.0, 1.5)},
        {'id': 'bit_order', 'desc': 'Bit order', 'default': 'lsb-first',
            'values': ('lsb-first', 'msb-first')},
        {'id': 'format', 'desc': 'Data format', 'default': 'ascii',
            'values': ('ascii', 'dec', 'hex', 'oct', 'bin')},
        {'id': 'invert', 'desc': 'Invert Signal?', 'default': 'no',
            'values': ('yes', 'no')},
    )
    annotations = (
        ('108', 'data', 'data'),
        ('7', 'start', 'start bits'),
        ('6', 'parity-ok', 'parity OK bits'),
        ('0', 'parity-err', 'parity error bits'),
        ('1', 'stop', 'stop bits'),
        ('1000', 'warnings', 'warnings'),
        ('209', 'data-bits', 'data bits'),
    )
    annotation_rows = (
        ('data', 'RX/TX', (0, 1, 2, 3, 4)),
        ('data-bits', 'Bits', (6,)),
        ('warnings', 'Warnings', (5,)),
    )
    binary = (
        ('rxtx', 'RX/TX dump'),
    )

    def put_ann_bit(self, width, data):
        s = self.bitstart
        self.put(floor(s), floor(s + width), self.out_ann, data)

    def put_python_bit(self, width, data):
        s = self.bitstart
        self.put(floor(s), floor(s + width), self.out_python, data)

    def put_ann_byte(self, data):
        ss, s = self.bytestart, self.bitstart
        self.put(floor(ss), floor(s + self.bit_width), self.out_ann, data)

    def put_python_byte(self, data):
        ss, s = self.bytestart, self.bitstart
        self.put(floor(ss), floor(s + self.bit_width), self.out_python, data)

    def put_binary_byte(self, data):
        ss, s = self.bytestart, self.bitstart
        self.put(floor(ss), floor(s + self.bit_width), self.out_binary, data)

    def __init__(self):
        self.samplerate = None
        self.samplenum = 0
        self.startbit = -1
        self.bitcount = 0
        self.databyte = 0
        self.paritybit = -1
        self.stopbit1 = -1
        self.bitstart = -1
        self.bytestart = -1
        self.state = 'FIND START'
        self.oldbit = -1
        self.databits = []

    def start(self):
        self.out_python = self.register(srd.OUTPUT_PYTHON)
        self.out_binary = self.register(srd.OUTPUT_BINARY)
        self.out_ann = self.register(srd.OUTPUT_ANN)
        if not self.samplerate:
            raise SamplerateError('Cannot decode without samplerate.')
        if self.samplerate < self.options['baudrate']*4:
            raise SamplerateError('Samplerate is too low for current baudrate setting, 4x at least!')
        if self.options['invert'] == 'yes':
            self.exp_logic = 1
        else:
            self.exp_logic = 0

    def metadata(self, key, value):
        if key == srd.SRD_CONF_SAMPLERATE:
            self.samplerate = value
            # The width of one UART bit in number of samples.
            self.bit_width = float(self.samplerate) / float(self.options['baudrate'])

    def frame_start(self, signal):
        # Save the sample number where the start bit begins.
        self.bitstart = self.samplenum
        self.state = 'GET START BIT'

    def get_start_bit(self, signal):
        self.startbit = signal

        # The startbit must be 0. If not, we report an error.
        if self.startbit != 0:
            self.put_python_bit(self.bit_width, ['INVALID STARTBIT', 0, self.startbit])
            self.put_ann_bit(self.bit_width, [5, ['Frame error', 'Frame err', 'FE']])
            # TODO: Abort? Ignore rest of the frame?

        self.bitcount = 0
        self.databyte = 0
        self.state = 'GET DATA BITS'

        self.put_python_bit(self.bit_width, ['STARTBIT', 0, self.startbit])
        self.put_ann_bit(self.bit_width, [1, ['Start bit', 'Start', 'S']])

    def get_data_bits(self, signal):
        # Save the sample number of where the bit begins.
        self.bitstart += self.bit_width
        if self.bitcount == 0 :
            self.bytestart = self.bitstart

        # Get the next data bit in LSB-first or MSB-first fashion.
        if self.options['bit_order'] == 'lsb-first':
            self.databyte >>= 1
            self.databyte |= \
                (signal << (self.options['num_data_bits'] - 1))
        else:
            self.databyte <<= 1
            self.databyte |= (signal << 0)

        self.put_ann_bit(self.bit_width, [6, ['%d' % signal]])

        # Store individual data bits and their start/end samplenumbers.
        s, halfbit = self.samplenum, int(self.bit_width / 2)
        self.databits.append([signal, s - halfbit, s + halfbit])

        # Return here, unless we already received all data bits.
        if self.bitcount < self.options['num_data_bits'] - 1:
            self.bitcount += 1
            return

        if self.options['parity_type'] == 'none':
            self.state = 'GET STOP BITS'
        else:
            self.state = 'GET PARITY BIT'

        self.put_python_byte(['DATA', 0, (self.databyte, self.databits)])

        b, f = self.databyte, self.options['format']
        if f == 'ascii':
            c = chr(b) if b in range(30, 126 + 1) else '[%02X]' % b
            self.put_ann_byte([0, [c]])
        elif f == 'dec':
            self.put_ann_byte([0, [str(b)]])
        elif f == 'hex':
            self.put_ann_byte([0, [hex(b)[2:].zfill(2).upper()]])
        elif f == 'oct':
            self.put_ann_byte([0, [oct(b)[2:].zfill(3)]])
        elif f == 'bin':
            self.put_ann_byte([0, [bin(b)[2:].zfill(8)]])

        #self.put_binary_byte([0, bytes([b])])
        #self.put_binary_byte([2, bytes([b])])

        self.databits = []

    def get_parity_bit(self, signal):
        # Save the sample number of where the bit begins.
        self.bitstart += self.bit_width
        self.paritybit = signal

        self.state = 'GET STOP BITS'
        if parity_ok(self.options['parity_type'], self.paritybit,
                     self.databyte, self.options['num_data_bits']):
            self.put_python_bit(self.bit_width, ['PARITYBIT', 0, self.paritybit])
            self.put_ann_bit(self.bit_width, [2, ['Parity bit', 'Parity', 'P']])
        else:
        #    # TODO: Return expected/actual parity values.
            self.put_python_bit(self.bit_width, ['PARITY ERROR', 0, (0, 1)]) # FIXME: Dummy tuple...
            self.put_ann_bit(self.bit_width, [3, ['Parity error', 'Parity err', 'PE']])

    # TODO: Currently only supports 1 stop bit.
    def get_stop_bits(self, signal):
        # Save the sample number of where the bit begins.
        self.bitstart += self.bit_width

        self.stopbit1 = signal
        # Stop bits must be 1. If not, we report an error.
        if self.stopbit1 != 1:
            self.put_python_bit(self.bit_width, ['INVALID STOPBIT', 0, self.stopbit1])
            self.put_ann_bit(self.bit_width, [5, ['Frame error', 'Frame err', 'FE']])
            # TODO: Abort? Ignore the frame? Other?

        self.state = 'FIND START'
        self.put_python_bit(int(self.bit_width * self.options['num_stop_bits']), ['STOPBIT', 0, self.stopbit1])
        self.put_ann_bit(int(self.bit_width * self.options['num_stop_bits']), [4, ['Stop bit', 'Stop', 'T']])

    def decode(self, ss, es, logic):
        for (self.samplenum, pins) in logic:

            # In default case, the iteration gap is 1
            logic.logic_mask = 0
            logic.edge_index = 0

            (signal,) = pins

            if self.options['invert'] == 'yes':
                signal = not signal

            # State machine.
            if self.state == 'FIND START':
                if (self.oldbit == 1 and signal == 0):
                    self.frame_start(signal)
                    logic.itercnt += (self.bit_width - 1) / 2.0
                else:
                    logic.exp_logic = self.exp_logic
                    logic.logic_mask = 1
                    logic.cur_pos = self.samplenum
                    signal = 1
            elif self.state == 'GET START BIT':
                self.get_start_bit(signal)
                logic.itercnt += self.bit_width
            elif self.state == 'GET DATA BITS':
                self.get_data_bits(signal)
                logic.itercnt += self.bit_width
            elif self.state == 'GET PARITY BIT':
                self.get_parity_bit(signal)
                logic.itercnt += self.bit_width
            elif self.state == 'GET STOP BITS':
                self.get_stop_bits(signal)
                logic.itercnt += (self.options['num_stop_bits'] - 0.75) * self.bit_width
                signal = 0

            # Save current RX/TX values for the next round.
            self.oldbit = signal
