##
## This file is part of the libsigrokdecode project.
##
## Copyright (C) 2016 Sean Burford <sburford@google.com>
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
from common.srdhelper import bits2int

class SamplerateError(Exception):
    pass

class Decoder(srd.Decoder):
    api_version = 3
    id = 'wiegand'
    name = 'Wiegand'
    longname = 'Wiegand interface'
    desc = 'Wiegand interface for electronic entry systems.'
    license = 'gplv2+'
    inputs = ['logic']
    outputs = []
    tags = ['Embedded/industrial', 'RFID']
    channels = (
        {'id': 'd0', 'name': 'D0', 'desc': 'Data 0 line', 'idn':'dec_wiegand_chan_d0'},
        {'id': 'd1', 'name': 'D1', 'desc': 'Data 1 line', 'idn':'dec_wiegand_chan_d1'},
    )
    options = (
        {'id': 'active', 'desc': 'Data lines active level',
         'default': 'low', 'values': ('low', 'high'), 'idn':'dec_wiegand_opt_active'},
        {'id': 'bitwidth_ms', 'desc': 'Single bit width in milliseconds',
         'default': 4, 'values': (1, 2, 4, 8, 16, 32), 'idn':'dec_wiegand_opt_bitwidth_ms'},
    )
    annotations = (
        ('bits', 'Bits'),
        ('state', 'State'),
    )
    annotation_rows = (
        ('bits', 'Binary value', (0,)),
        ('state', 'Stream state', (1,)),
    )

    def __init__(self):
        self.reset()

    def reset(self):
        self.samplerate = None
        self._samples_per_bit = 10

        self._d0_prev = None
        self._d1_prev = None

        self._state = None
        self.ss_state = None

        self.ss_bit = None
        self.es_bit = None
        self._bit = None
        self._bits = []

    def start(self):
        'Register output types and verify user supplied decoder values.'
        self.out_ann = self.register(srd.OUTPUT_ANN)
        self._active = 1 if self.options['active'] == 'high' else 0
        self._inactive = 1 - self._active

    def metadata(self, key, value):
        'Receive decoder metadata about the data stream.'
        if key == srd.SRD_CONF_SAMPLERATE:
            self.samplerate = value
            if self.samplerate:
                ms_per_sample = 1000 * (1.0 / self.samplerate)
                ms_per_bit = float(self.options['bitwidth_ms'])
                self._samples_per_bit = int(max(1, int(ms_per_bit / ms_per_sample)))

    def _update_state(self, state, bit=None):
        'Update state and bit values when they change.'
        if self._bit is not None:
            self._bits.append(self._bit)
            self.put(self.ss_bit, self.samplenum, self.out_ann,
                     [0, [str(self._bit)]])
        self._bit = bit
        self.ss_bit = self.samplenum
        if bit is not None:
            # Set a timeout so that the final bit ends.
            self.es_bit = self.samplenum + self._samples_per_bit
        else:
            self.es_bit = None

        if state != self._state:
            ann = None
            if self._state == 'data':
                accum_bits = ''.join(str(x) for x in self._bits)
                bstart = -1 #int(self.options['bit-start'])
                bits = self._bits
                blen = len(self._bits)

                # get data block
                if bstart != -1: 
                    bits = []
                    while bstart + 4 <= blen:
                        bits.append(self._bits[bstart+0]);
                        bits.append(self._bits[bstart+1]);
                        bits.append(self._bits[bstart+2]);
                        bits.append(self._bits[bstart+3]);
                        bstart += 4

                blen = len(bits)
                #s1 = '%d bits {$}' % blen
                s1 = '%d bits %s' % (blen, accum_bits)
                s2 = '%d bits' % blen
                #s3 = '@%02X' % (bits2int(bits))
                #ann = [1, [s1, s2, s3]]
                ann = [1, [s1, s2]]
            elif self._state == 'invalid':
                ann = [1, [self._state]]
            if ann:
                self.put(self.ss_state, self.samplenum, self.out_ann, ann)
            self.ss_state = self.samplenum
            self._state = state
            self._bits = []

    def decode(self):
        if not self.samplerate:
            raise SamplerateError('Cannot decode without samplerate.')
        while True:
            # TODO: Come up with more appropriate self.wait() conditions.
            (d0, d1) = self.wait()

            if d0 == self._d0_prev and d1 == self._d1_prev:
                if self.es_bit and self.samplenum >= self.es_bit:
                    if (d0, d1) == (self._inactive, self._inactive):
                        self._update_state('idle')
                    else:
                        self._update_state('invalid')
                continue

            if self._state in (None, 'idle', 'data'):
                if (d0, d1) == (self._active, self._inactive):
                    self._update_state('data', 0)
                elif (d0, d1) == (self._inactive, self._active):
                    self._update_state('data', 1)
                elif (d0, d1) == (self._active, self._active):
                    self._update_state('invalid')
            elif self._state == 'invalid':
                # Wait until we see an idle state before leaving invalid.
                # This prevents inverted lines from being misread.
                if (d0, d1) == (self._inactive, self._inactive):
                    self._update_state('idle')

            self._d0_prev, self._d1_prev = d0, d1

    def report(self):
        return '%s: %s D0 %d D1 %d (active on %d), %d samples per bit' % (
            self.name, self._state, self._d0_prev, self._d1_prev,
            self._active, self._samples_per_bit)
