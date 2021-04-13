#
#   ©2021 Janusz Kostorz
#   All rights reserved.
#
#   This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 2 of the License, or (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#   See the GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License along with this program; if not, see <http://www.gnu.org/licenses/>.
#

import sigrokdecode as srd
from common.srdhelper import bitpack
from .lists import *


class SamplerateError(Exception):
    pass


class Pin:
    RI, = range(1)


class Ann:
    BitStart, BitCode, BitStop, Header, Code, Footer, Decode = range(7)


class Decoder(srd.Decoder):
    api_version = 3
    id = 'onkyo_ri'
    name = 'Onkyo RI'
    longname = 'Onkyo Remote Interactive'
    desc = 'Onkyo RI audio equipment protocol.'
    license = 'gplv2+'
    inputs = ['logic']
    outputs = []
    tags = ['RI']
    channels = ({'id': 'ri', 'name': 'RI', 'desc': 'Onkyo RI'}, )
    options = ({
        'id': 'polarity',
        'desc': 'Polarity',
        'default': 'active-high',
        'values': ('auto', 'active low', 'active high')
    }, )
    annotations = (
        ('bitstart', 'Start'),
        ('bitcode', 'Code'),
        ('bitstop', 'Stop'),
        ('header', 'Header'),
        ('code', 'Hex'),
        ('footer', 'Footer'),
        ('decode', 'Decode'),
    )
    annotation_rows = (
        ('bits', 'Bits', (
            Ann.BitStart,
            Ann.BitCode,
            Ann.BitStop,
        )),
        ('codes', 'Code', (
            Ann.Header,
            Ann.Code,
            Ann.Footer,
        )),
        ('decodes', 'Decode', (Ann.Decode, )),
    )

    def __init__(self):
        self.reset()

    def start(self):
        self.idle = int(self.samplerate * 7e-3) - 1  # Idle
        self.header = int(self.samplerate * 4e-3) - 1  # Header bit
        self.lo = int(self.samplerate * 2e-3) - 1  # Lo bit
        self.hi = int(self.samplerate * 3e-3) - 1  # Hi bit
        self.stop = int(self.samplerate * 7e-3) - 1  # Stop bit
        self.anchor = self.register(srd.OUTPUT_ANN)  # Anchor for outputs

    def reset(self):
        self.state = 'Idle'
        self.samplenum_save = self.samplenum_field = self.samplenum_decode = 0
        self.data = []
        self.hex = None

    def metadata(self, key, value):
        if key == srd.SRD_CONF_SAMPLERATE:
            self.samplerate = value

    # Compare with 0.1 tolerance
    def tolerance(self, real, base):
        return (real > base * 0.9 and real < base * 1.1)

    def decode(self):

        # Wait for samples
        if not self.samplerate:
            raise SamplerateError('No input data')

        # Polarity config auto (sample 0 as reference)
        if self.options['polarity'] == 'auto':
            level, = self.wait({'skip': 0})
            active = 1 - level

        # Polarity config manual
        else:
            active = 0 if self.options['polarity'] == 'active low' else 1

        # Main
        while True:

            # Wait for pin change
            self.ri, = self.wait({Pin.RI: 'e'})

            # Pin active
            if self.ri != active:
                if self.state != 'Stop':
                    continue

            # Long idle reset
            space = self.samplenum - self.samplenum_save
            if self.state != 'Stop' and space > self.idle:
                self.reset()

            # Start bit
            if self.state == 'Idle':
                if self.tolerance(space, self.header):
                    self.put(self.samplenum_field, self.samplenum, self.anchor,
                             [Ann.BitStart, ['Start', 'S']])
                    self.put(self.samplenum_field, self.samplenum, self.anchor,
                             [Ann.Header, ['Start', 'S']])
                    self.samplenum_decode = self.samplenum_field
                    self.data = []
                    self.state = 'Code'
                self.samplenum_save = self.samplenum_field = self.samplenum

            # Code bits
            elif self.state == 'Code':
                bit = None
                if self.tolerance(space, self.lo):
                    bit = 0
                elif self.tolerance(space, self.hi):
                    bit = 1
                if bit in (0, 1):
                    self.put(self.samplenum_save, self.samplenum, self.anchor,
                             [Ann.BitCode, ['{:d}'.format(bit)]])
                    self.data.insert(0, bit)
                else:
                    self.reset()
                self.samplenum_save = self.samplenum

                # Code complete
                if len(self.data) == 12:

                    code = bitpack(self.data)

                    f = '0x{{:0{}X}}'.format(3)
                    self.put(self.samplenum_field, self.samplenum, self.anchor,
                             [Ann.Code, [f.format(code)]])

                    self.hex = code
                    self.samplenum_field = self.samplenum
                    self.state = 'Stop'

            # Stop
            elif self.state == 'Stop':
                self.put(self.samplenum_field, self.samplenum_save + self.stop,
                         self.anchor, [Ann.BitStop, ['Stop', 'S']])
                self.put(self.samplenum_field, self.samplenum_save + self.stop,
                         self.anchor, [Ann.Footer, ['Stop', 'S']])

                oricode = ori.get(self.hex, 'Unknown RI code')
                self.put(self.samplenum_decode,
                         self.samplenum_save + self.stop, self.anchor,
                         [Ann.Decode, [
                             '{}'.format(oricode),
                         ]])

                self.samplenum_save = self.samplenum_field = self.samplenum
                self.state = 'Idle'
