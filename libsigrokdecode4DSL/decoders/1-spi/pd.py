##
## This file is part of the libsigrokdecode project.
##
## Copyright (C) 2011 Gareth McMullin <gareth@blacksphere.co.nz>
## Copyright (C) 2012-2014 Uwe Hermann <uwe@hermann-uwe.de>
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
from collections import namedtuple

Data = namedtuple('Data', ['ss', 'es', 'val'])

'''
OUTPUT_PYTHON format:

Packet:
[<ptype>, <data1>, <data2>]

<ptype>:
 - 'DATA': <data1> contains the MOSI data, <data2> contains the MISO data.
   The data is _usually_ 8 bits (but can also be fewer or more bits).
   Both data items are Python numbers (not strings), or None if the respective
   channel was not supplied.
 - 'BITS': <data1>/<data2> contain a list of bit values in this MOSI/MISO data
   item, and for each of those also their respective start-/endsample numbers.
 - 'CS-CHANGE': <data1> is the old CS# pin value, <data2> is the new value.
   Both data items are Python numbers (0/1), not strings. At the beginning of
   the decoding a packet is generated with <data1> = None and <data2> being the
   initial state of the CS# pin or None if the chip select pin is not supplied.
 - 'TRANSFER': <data1>/<data2> contain a list of Data() namedtuples for each
   byte transferred during this block of CS# asserted time. Each Data() has
   fields ss, es, and val.

Examples:
 ['CS-CHANGE', None, 1]
 ['CS-CHANGE', 1, 0]
 ['DATA', 0xff, 0x3a]
 ['BITS', [[1, 80, 82], [1, 83, 84], [1, 85, 86], [1, 87, 88],
           [1, 89, 90], [1, 91, 92], [1, 93, 94], [1, 95, 96]],
          [[0, 80, 82], [1, 83, 84], [0, 85, 86], [1, 87, 88],
           [1, 89, 90], [1, 91, 92], [0, 93, 94], [0, 95, 96]]]
 ['DATA', 0x65, 0x00]
 ['DATA', 0xa8, None]
 ['DATA', None, 0x55]
 ['CS-CHANGE', 0, 1]
 ['TRANSFER', [Data(ss=80, es=96, val=0xff), ...],
              [Data(ss=80, es=96, val=0x3a), ...]]
'''

# Key: (CPOL, CPHA). Value: SPI mode.
# Clock polarity (CPOL) = 0/1: Clock is low/high when inactive.
# Clock phase (CPHA) = 0/1: Data is valid on the leading/trailing clock edge.
spi_mode = {
    (0, 0): 0, # Mode 0
    (0, 1): 1, # Mode 1
    (1, 0): 2, # Mode 2
    (1, 1): 3, # Mode 3
}

class SamplerateError(Exception):
    pass

class ChannelError(Exception):
    pass

class Decoder(srd.Decoder):
    api_version = 2
    id = '1:spi'
    name = '1:SPI'
    longname = 'Serial Peripheral Interface'
    desc = 'Full-duplex, synchronous, serial bus.'
    license = 'gplv2+'
    inputs = ['logic']
    outputs = ['spi']
    channels = (
        {'id': 'clk', 'type': 0, 'name': 'CLK', 'desc': 'Clock'},
    )
    optional_channels = (
        {'id': 'miso', 'type': 107, 'name': 'MISO', 'desc': 'Master in, slave out'},
        {'id': 'mosi', 'type': 109, 'name': 'MOSI', 'desc': 'Master out, slave in'},
        {'id': 'cs', 'type': -1, 'name': 'CS#', 'desc': 'Chip-select'},
    )
    options = (
        {'id': 'cs_polarity', 'desc': 'CS# polarity', 'default': 'active-low',
            'values': ('active-low', 'active-high')},
        {'id': 'cpol', 'desc': 'Clock polarity', 'default': 0,
            'values': (0, 1)},
        {'id': 'cpha', 'desc': 'Clock phase', 'default': 0,
            'values': (0, 1)},
        {'id': 'bitorder', 'desc': 'Bit order',
            'default': 'msb-first', 'values': ('msb-first', 'lsb-first')},
        {'id': 'wordsize', 'desc': 'Word size', 'default': 8},
    )
    annotations = (
        ('106', 'miso-data', 'MISO data'),
        ('108', 'mosi-data', 'MOSI data'),
        ('207', 'miso-bits', 'MISO bits'),
        ('209', 'mosi-bits', 'MOSI bits'),
        ('1000', 'warnings', 'Human-readable warnings'),
    )
    annotation_rows = (
        ('miso-data', 'MISO data', (0,)),
        ('miso-bits', 'MISO bits', (2,)),
        ('mosi-data', 'MOSI data', (1,)),
        ('mosi-bits', 'MOSI bits', (3,)),
        #('other', 'Other', (4,)),
    )
    binary = (
        ('miso', 'MISO'),
        ('mosi', 'MOSI'),
    )

    def __init__(self):
        self.samplerate = None
        self.oldclk = -1
        self.bitcount = 0
        self.misodata = self.mosidata = 0
        self.misobits = []
        self.mosibits = []
        self.misobytes = []
        self.mosibytes = []
        self.ss_block = -1
        self.samplenum = -1
        self.ss_transfer = -1
        self.cs_was_deasserted = False
        self.oldcs = None
        self.oldpins = None
        self.have_cs = self.have_miso = self.have_mosi = None
        self.no_cs_notification = False
        self.mode = None
        self.active_low = None
        self.pin_checked = False
        self.ws = None
        self.bitwidth = 0

    def metadata(self, key, value):
        if key == srd.SRD_CONF_SAMPLERATE:
            self.samplerate = value

    def start(self):
        self.out_python = self.register(srd.OUTPUT_PYTHON)
        self.out_ann = self.register(srd.OUTPUT_ANN)
        self.out_binary = self.register(srd.OUTPUT_BINARY)
        self.out_bitrate = self.register(srd.OUTPUT_META,
                meta=(int, 'Bitrate', 'Bitrate during transfers'))
        if not self.samplerate:
            raise SamplerateError('Cannot decode without samplerate.')
        #Sample data on rising/falling clock edge (depends on mode).
        self.mode = spi_mode[self.options['cpol'], self.options['cpha']]
        self.active_low = (self.options['cs_polarity'] == 'active-low')
        self.ws = self.options['wordsize']

    def putw(self, data):
        self.put(self.ss_block, self.samplenum, self.out_ann, data)

    def putdata(self):
        # Pass MISO and MOSI bits and then data to the next PD up the stack.
        if self.have_miso:
            ss, es = self.misobits[-1][1], self.misobits[0][2]
            self.put(ss, es, self.out_python, ['BITS', self.mosibits, self.misobits])
            self.misobytes.append(Data(ss=ss, es=es, val=self.misodata))
            for bit in reversed(self.misobits):
                self.put(bit[1], bit[2], self.out_ann, [2, ['%d' % bit[0]]])
            self.put(ss, es, self.out_ann, [0, ['%02X' % self.misodata]])
        #    self.put(ss, es, self.out_binary, [0, bytes([self.misodata])])
        if self.have_mosi:
            ss, es = self.mosibits[-1][1], self.mosibits[0][2]
            self.put(ss, es, self.out_python, ['DATA', self.mosidata, self.misodata])
            self.mosibytes.append(Data(ss=ss, es=es, val=self.mosidata))
            for bit in reversed(self.mosibits):
                self.put(bit[1], bit[2], self.out_ann, [3, ['%d' % bit[0]]])
            self.put(ss, es, self.out_ann, [1, ['%02X' % self.mosidata]])
        #    self.put(ss, es, self.out_binary, [1, bytes([self.mosibits])])

    def reset_decoder_state(self):
        self.misodata = 0 if self.have_miso else None
        self.mosidata = 0 if self.have_mosi else None
        self.misobits = [] if self.have_miso else None
        self.mosibits = [] if self.have_mosi else None
        self.bitcount = 0

    def handle_bit(self, miso, mosi, clk, cs):
        # If this is the first bit of a dataword, save its sample number.
        if self.bitcount == 0:
            self.ss_block = self.samplenum
            self.cs_was_deasserted = (cs == self.deasserted_cs)
        if self.bitcount == 1:
            self.bitwidth = self.samplenum - self.ss_block

        shift_cnt = (self.ws - 1 - self.bitcount) if (self.options['bitorder'] == 'msb-first') else self.bitcount
        # Receive MISO bit into our shift register.
        es = self.samplenum
        if self.have_miso:
            self.misodata |= miso << shift_cnt
            if self.bitcount > 0:
                es += self.samplenum - self.misobits[0][1]
            self.misobits.insert(0, [miso, self.samplenum, es])
            if self.bitcount > 0:
                self.misobits[1][2] = self.samplenum
        # Receive MOSI bit into our shift register.
        es = self.samplenum
        if self.have_mosi:
            self.mosidata |= mosi << shift_cnt
            if self.bitcount > 0:
                es += self.samplenum - self.mosibits[0][1]
            self.mosibits.insert(0, [mosi, self.samplenum, es])
            if self.bitcount > 0:
                self.mosibits[1][2] = self.samplenum

        self.bitcount += 1
        # Continue to receive if not enough bits were received, yet.
        if self.bitcount != self.ws:
            return

        self.putdata()

        # Meta bitrate.
        #elapsed = 1 / float(self.samplerate)
        #elapsed *= (self.samplenum - self.ss_block + 1)
        #bitrate = int(1 / elapsed * self.options['wordsize'])
        #self.put(self.ss_block, self.samplenum, self.out_bitrate, bitrate)

        #if self.have_cs and self.cs_was_deasserted:
        #    self.putw([4, ['CS# was deasserted during this data word!']])

        self.reset_decoder_state()

    def decode(self, ss, es, logic):
        # Either MISO or MOSI can be omitted (but not both). CS# is optional.
        for (self.samplenum, pins) in logic:
            (clk, miso, mosi, cs) = pins
            if not self.pin_checked:
                self.have_miso = (miso in (0, 1))
                self.have_mosi = (mosi in (0, 1))
                self.have_cs = (cs in (0, 1))
                # Either MISO or MOSI (but not both) can be omitted.
                if not (self.have_miso or self.have_mosi):
                    raise ChannelError('Either MISO or MOSI (or both) pins required.')
                if (self.mode == 0 or self.mode == 3):
                    self.exp_oldclk = 0
                    self.exp_clk = 1
                else:
                    self.exp_oldclk = 1
                    self.exp_clk = 0
                self.logic_mask = 0b1001 if self.have_cs else 0b0001
                self.exp_logic = 0b0000 if self.active_low else 0b1000
                self.asserted_oldcs = 1 if self.active_low else 0
                self.asserted_cs = 0 if self.active_low else 1
                self.deasserted_oldcs = 0 if self.active_low else 1
                self.deasserted_cs = 1 if self.active_low else 0
                self.pin_checked = True

            logic.logic_mask = self.logic_mask
            logic.cur_pos = self.samplenum
            logic.edge_index = -1

            # Tell stacked decoders that we don't have a CS# signal.
            if not self.no_cs_notification and not self.have_cs:
                self.put(0, 0, self.out_python, ['CS-CHANGE', None, None])
                self.no_cs_notification = True

            if (self.oldcs, cs) == (self.asserted_oldcs, self.asserted_cs):
                self.ss_transfer = self.samplenum
                self.misobytes = []
                self.mosibytes = []
                self.reset_decoder_state()
            elif (self.oldcs, cs) == (self.deasserted_oldcs, self.deasserted_cs):
                self.put(self.ss_transfer, self.samplenum, self.out_python,
                    ['TRANSFER', self.mosibytes, self.misobytes])
                logic.exp_logic = self.exp_logic
                cs = self.asserted_oldcs
                logic.logic_mask = 0b1000
                logic.edge_index = 3
            elif not self.have_cs or cs == self.asserted_cs:
                if (self.oldclk, clk) == (self.exp_oldclk, self.exp_clk):
                    #Sample on rising/falling clock edge
                    self.handle_bit(miso, mosi, clk, cs)

            self.oldclk, self.oldcs = clk, cs
