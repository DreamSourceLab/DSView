##
## This file is part of the libsigrokdecode project.
##
## Copyright (C) 2011-2014 Uwe Hermann <uwe@hermann-uwe.de>
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

import sigrokdecode as srd
from common.srdhelper import bitpack
from math import floor, ceil

'''
OUTPUT_PYTHON format:

Packet:
[<ptype>, <rxtx>, <pdata>]

This is the list of <ptype>s and their respective <pdata> values:
 - 'STARTBIT': The data is the (integer) value of the start bit (0/1).
 - 'DATA': This is always a tuple containing two items:
   - 1st item: the (integer) value of the UART data. Valid values
     range from 0 to 511 (as the data can be up to 9 bits in size).
   - 2nd item: the list of individual data bits and their ss/es numbers.
 - 'PARITYBIT': The data is the (integer) value of the parity bit (0/1).
 - 'STOPBIT': The data is the (integer) value of the stop bit (0 or 1).
 - 'INVALID STARTBIT': The data is the (integer) value of the start bit (0/1).
 - 'INVALID STOPBIT': The data is the (integer) value of the stop bit (0/1).
 - 'PARITY ERROR': The data is a tuple with two entries. The first one is
   the expected parity value, the second is the actual parity value.
 - 'FRAME': The data is always a tuple containing two items: The (integer)
   value of the UART data, and a boolean which reflects the validity of the
   UART frame.

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

class ChannelError(Exception):
    pass

class Decoder(srd.Decoder):
    api_version = 3
    id = '0:uart'
    name = '0:UART'
    longname = 'Universal Asynchronous Receiver/Transmitter'
    desc = 'Asynchronous, serial bus.'
    license = 'gplv2+'
    inputs = ['logic']
    outputs = ['uart']
    tags = ['Embedded/industrial']
    channels = (
        {'id': 'rxtx', 'type': 209, 'name': 'RX/TX', 'desc': 'UART transceive line'},
    )
    options = (
        {'id': 'baudrate', 'desc': 'Baud rate', 'default': 115200},
        {'id': 'num_data_bits', 'desc': 'Data bits', 'default': 8,
            'values': tuple(range(4,129,1))},
        {'id': 'parity_type', 'desc': 'Parity type', 'default': 'none',
            'values': ('none', 'odd', 'even', 'zero', 'one')},
        {'id': 'parity_check', 'desc': 'Check parity?', 'default': 'yes',
            'values': ('yes', 'no')},
        {'id': 'num_stop_bits', 'desc': 'Stop bits', 'default': 1.0,
            'values': (0.0, 0.5, 1.0, 1.5, 2.0, 2.5)},
        {'id': 'bit_order', 'desc': 'Bit order', 'default': 'lsb-first',
            'values': ('lsb-first', 'msb-first')},
        {'id': 'format', 'desc': 'Data format', 'default': 'hex',
            'values': ('ascii', 'dec', 'hex', 'oct', 'bin')},
        {'id': 'invert', 'desc': 'Invert Signal?', 'default': 'no',
            'values': ('yes', 'no')},
	{'id': 'anno_startstop', 'desc': 'Display Start/Stop?', 'default': 'no',
            'values': ('yes', 'no')},
    )
    annotations = (
        ('108', 'data', 'data'),
        ('7', 'start', 'start bits'),
        ('6', 'parity-ok', 'parity OK bits'),
        ('0', 'parity-err', 'parity error bits'),
        ('1', 'stop', 'stop bits'),
        ('1000', 'warnings', 'warnings'),
    )
    annotation_rows = (
        ('data', 'RX/TX', (0, 1, 2, 3, 4)),
        ('warnings', 'Warnings', (5,)),
    )
    idle_state = 'WAIT FOR START BIT'

    def putx(self, data):
        s, halfbit = self.startsample, self.bit_width / 2.0
        if self.options['anno_startstop'] == 'yes' :
            self.put(s - floor(halfbit), self.samplenum + ceil(halfbit), self.out_ann, data)
        else :
            self.put(self.frame_start, self.samplenum + ceil(halfbit * (1+self.options['num_stop_bits'])), self.out_ann, data)

    def putg(self, data):
        s, halfbit = self.samplenum, self.bit_width / 2.0
        self.put(s - floor(halfbit), s + ceil(halfbit), self.out_ann, data)

    def putgse(self, ss, es, data):
        self.put(ss, es, self.out_ann, data)

    def __init__(self):
        self.reset()

    def reset(self):
        self.samplerate = None
        self.samplenum = 0
        self.frame_start = -1
        self.frame_valid = None
        self.startbit = -1
        self.cur_data_bit = 0
        self.datavalue = 0
        self.paritybit = -1
        self.stopbit1 = -1
        self.startsample = -1
        self.state = 'WAIT FOR START BIT'
        self.databits = []

    def start(self):
        self.out_ann = self.register(srd.OUTPUT_ANN)
        self.bw = (self.options['num_data_bits'] + 7) // 8

    def metadata(self, key, value):
        if key == srd.SRD_CONF_SAMPLERATE:
            self.samplerate = value
            # The width of one UART bit in number of samples.
            self.bit_width = float(self.samplerate) / float(self.options['baudrate'])

    def get_sample_point(self, bitnum):
        # Determine absolute sample number of a bit slot's sample point.
        # bitpos is the samplenumber which is in the middle of the
        # specified UART bit (0 = start bit, 1..x = data, x+1 = parity bit
        # (if used) or the first stop bit, and so on).
        # The samples within bit are 0, 1, ..., (bit_width - 1), therefore
        # index of the middle sample within bit window is (bit_width - 1) / 2.
        bitpos = self.frame_start + (self.bit_width - 1) / 2.0
        bitpos += bitnum * self.bit_width
        return bitpos

    def wait_for_start_bit(self, signal):
        # Save the sample number where the start bit begins.
        self.frame_start = self.samplenum
        self.frame_valid = True

        self.state = 'GET START BIT'

    def get_start_bit(self, signal):
        self.startbit = signal

        # The startbit must be 0. If not, we report an error and wait
        # for the next start bit (assuming this one was spurious).
        if self.startbit != 0:
            self.putg([5, ['Frame error', 'Frame err', 'FE']])
            self.frame_valid = False
            es = self.samplenum + ceil(self.bit_width / 2.0)
            self.state = 'WAIT FOR START BIT'
            return

        self.cur_data_bit = 0
        self.datavalue = 0
        self.startsample = -1

        if self.options['anno_startstop'] == 'yes':
            self.putg([1, ['Start bit', 'Start', 'S']])

        self.state = 'GET DATA BITS'

    def get_data_bits(self, signal):
        # Save the sample number of the middle of the first data bit.
        if self.startsample == -1:
            self.startsample = self.samplenum

        # Store individual data bits and their start/end samplenumbers.
        s, halfbit = self.samplenum, int(self.bit_width / 2)
        self.databits.append([signal, s - halfbit, s + halfbit])

        # Return here, unless we already received all data bits.
        self.cur_data_bit += 1
        if self.cur_data_bit < self.options['num_data_bits']:
            return

        # Convert accumulated data bits to a data value.
        bits = [b[0] for b in self.databits]
        if self.options['bit_order'] == 'msb-first':
            bits.reverse()
        self.datavalue = bitpack(bits)

        b = self.datavalue
        formatted = self.format_value(b)
        if formatted is not None:
            self.putx([0, [formatted]])

        self.databits = []

        # Advance to either reception of the parity bit, or reception of
        # the STOP bits if parity is not applicable.
        self.state = 'GET PARITY BIT'
        if self.options['parity_type'] == 'none':
            self.state = 'GET STOP BITS'

    def format_value(self, v):
        # Format value 'v' according to configured options.
        # Reflects the user selected kind of representation, as well as
        # the number of data bits in the UART frames.

        fmt, bits = self.options['format'], self.options['num_data_bits']

        # Assume "is printable" for values from 32 to including 126,
        # below 32 is "control" and thus not printable, above 127 is
        # "not ASCII" in its strict sense, 127 (DEL) is not printable,
        # fall back to hex representation for non-printables.
        if fmt == 'ascii':
            if v in range(32, 126 + 1):
                return chr(v)
            hexfmt = "[{:02X}]" if bits <= 8 else "[{:03X}]"
            return hexfmt.format(v)

        # Mere number to text conversion without prefix and padding
        # for the "decimal" output format.
        if fmt == 'dec':
            return "{:d}".format(v)

        # Padding with leading zeroes for hex/oct/bin formats, but
        # without a prefix for density -- since the format is user
        # specified, there is no ambiguity.
        if fmt == 'hex':
            digits = (bits + 4 - 1) // 4
            fmtchar = "X"
        elif fmt == 'oct':
            digits = (bits + 3 - 1) // 3
            fmtchar = "o"
        elif fmt == 'bin':
            digits = bits
            fmtchar = "b"
        else:
            fmtchar = None
        if fmtchar is not None:
            fmt = "{{:0{:d}{:s}}}".format(digits, fmtchar)
            return fmt.format(v)

        return None

    def get_parity_bit(self, signal):
        self.paritybit = signal

        if parity_ok(self.options['parity_type'], self.paritybit,
                     self.datavalue, self.options['num_data_bits']):
            self.putg([2, ['Parity bit', 'Parity', 'P']])
        else:
            # TODO: Return expected/actual parity values.
            self.putg([3, ['Parity error', 'Parity err', 'PE']])
            self.frame_valid = False

        self.state = 'GET STOP BITS'

    # TODO: Currently only supports 1 stop bit.
    def get_stop_bits(self, signal):
        self.stopbit1 = signal

        # Stop bits must be 1. If not, we report an error.
        if self.stopbit1 != 1:
            self.putg([5, ['Frame error', 'Frame err', 'FE']])
            self.frame_valid = False

        if self.options['anno_startstop'] == 'yes':
            self.putg([2, ['Stop bit', 'Stop', 'T']])

        # Pass the complete UART frame to upper layers.
        es = self.samplenum + ceil(self.bit_width / 2.0)

        self.state = 'WAIT FOR START BIT'

    def get_wait_cond(self, inv):
        # Return condititions that are suitable for Decoder.wait(). Those
        # conditions either match the falling edge of the START bit, or
        # the sample point of the next bit time.
        state = self.state
        if state == 'WAIT FOR START BIT':
            return {0: 'r' if inv else 'f'}
        if state == 'GET START BIT':
            bitnum = 0
        elif state == 'GET DATA BITS':
            bitnum = 1 + self.cur_data_bit
        elif state == 'GET PARITY BIT':
            bitnum = 1 + self.options['num_data_bits']
        elif state == 'GET STOP BITS':
            bitnum = 1 + self.options['num_data_bits']
            bitnum += 0 if self.options['parity_type'] == 'none' else 1
        want_num = ceil(self.get_sample_point(bitnum))
        return {'skip': want_num - self.samplenum}

    def inspect_sample(self, signal, inv):
        # Inspect a sample returned by .wait() for the specified UART line.
        if inv:
            signal = not signal

        state = self.state
        if state == 'WAIT FOR START BIT':
            self.wait_for_start_bit(signal)
        elif state == 'GET START BIT':
            self.get_start_bit(signal)
        elif state == 'GET DATA BITS':
            self.get_data_bits(signal)
        elif state == 'GET PARITY BIT':
            self.get_parity_bit(signal)
        elif state == 'GET STOP BITS':
            self.get_stop_bits(signal)

    def decode(self):
        if not self.samplerate:
            raise SamplerateError('Cannot decode without samplerate.')

        inv = self.options['invert'] == 'yes'

        while True:
            conds = self.get_wait_cond(inv)
            (rxtx, ) = self.wait(conds)
            if (self.matched & (0b1 << 0)):
                self.inspect_sample(rxtx, inv)
