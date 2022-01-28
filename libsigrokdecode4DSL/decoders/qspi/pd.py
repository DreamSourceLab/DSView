###
## This file is part of the libsigrokdecode project.
##
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

import sigrokdecode as srd
from collections import namedtuple

Data = namedtuple('Data', ['ss', 'es', 'val'])


# Key: (CPOL, CPHA). Value: SPI mode.
# Clock polarity (CPOL) = 0/1: Clock is low/high when inactive.
# Clock phase (CPHA) = 0/1: Data is valid on the leading/trailing clock edge.
spi_mode = {
    (0, 0): 0, # Mode 0
    (0, 1): 1, # Mode 1
    (1, 0): 2, # Mode 2
    (1, 1): 3, # Mode 3
}

class ChannelError(Exception):
    pass

class Decoder(srd.Decoder):
    api_version = 3
    id = 'qspi'
    name = 'QSPI'
    longname = 'Quad Serial Peripheral Interface'
    desc = 'Full-duplex, synchronous, serial bus.'
    license = 'gplv2+'
    inputs = ['logic']
    outputs = ['spi']
    tags = ['Embedded/industrial']
    channels = (
        {'id': 'clk', 'type': 0, 'name': 'CLK', 'desc': 'Clock'},
        {'id': 'io0', 'type': 107, 'name': 'IO0', 'desc': 'Data i/o 0'},
    )
    optional_channels = (
        {'id': 'io1', 'type': 107, 'name': 'IO1', 'desc': 'Data i/o 1'},
        {'id': 'io2', 'type': 107, 'name': 'IO2', 'desc': 'Data i/o 2'},
        {'id': 'io3', 'type': 107, 'name': 'IO3', 'desc': 'Data i/o 3'},
        {'id': 'cs', 'type': -1, 'name': 'CS#', 'desc': 'Chip-select'},
    )
    options = (
        {'id': 'cs_polarity', 'desc': 'CS# polarity', 'default': 'active-low',
            'values': ('active-low', 'active-high')},
        {'id': 'cpol', 'desc': 'Clock polarity (CPOL)', 'default': 0,
            'values': (0, 1)},
        {'id': 'cpha', 'desc': 'Clock phase (CPHA)', 'default': 0,
            'values': (0, 1)},
        {'id': 'bitorder', 'desc': 'Bit order',
            'default': 'msb-first', 'values': ('msb-first', 'lsb-first')},
        {'id': 'wordsize', 'desc': 'Word size', 'default': 8},
    )
    annotations = (
        ('106', 'data', 'data'),
    )
    annotation_rows = (
        ('data', 'data', (0,)),
    )

    def __init__(self):
        self.reset()

    def reset(self):
        self.samplerate = None
        self.bitcount = 0
        self.data = 0
        self.bits = []
        self.ss_block = -1
        self.samplenum = -1
        self.ss_transfer = -1
        self.cs_was_deasserted = False
        self.have_cs = self.have_io1 = self.have_io3 = None

    def start(self):
        self.out_ann = self.register(srd.OUTPUT_ANN)
        self.bw = (self.options['wordsize'] + 7) // 8

    def metadata(self, key, value):
       if key == srd.SRD_CONF_SAMPLERATE:
            self.samplerate = value

    def putw(self, data):
        self.put(self.ss_block, self.samplenum, self.out_ann, data)

    def putdata(self):
        # Pass bits and then data to the next PD up the stack.
        ss, es = self.bits[-1][1], self.bits[0][2]

        # Dataword annotations.
        self.put(ss, es, self.out_ann, [0, ['%02X' % self.data]])

    def reset_decoder_state(self):
        self.data = 0
        self.bits = []
        self.bitcount = 0

    def cs_asserted(self, cs):
        active_low = (self.options['cs_polarity'] == 'active-low')
        return (cs == 0) if active_low else (cs == 1)

    def handle_bit(self, datapins, clk, cs):
        # If this is the first bit of a dataword, save its sample number.
        if self.bitcount == 0:
            self.ss_block = self.samplenum
            self.cs_was_deasserted = \
                not self.cs_asserted(cs) if self.have_cs else False

        bo = self.options['bitorder']
        ws = self.options['wordsize']
        if self.have_io3:
            nibws = ws >> 2
        elif self.have_io1:
            nibws = ws >> 1
        else:
            nibws = ws

        # Receive bit into our shift register.
        if self.have_io3:
            for i in range(4):
                if bo == 'msb-first':
                    self.data |= datapins[i] << (ws - 1 - self.bitcount*4 - i)
                else:
                    self.data |= datapins[3-i] << (self.bitcount*4 + i)
        elif self.have_io1:
            for i in range(2):
                if bo == 'msb-first':
                    self.data |= datapins[i+2] << (ws - 1 - self.bitcount*2 - i)
                else:
                    self.data |= datapins[3-i] << (self.bitcount*2 + i)
        else:
            if bo == 'msb-first':
                self.data |= datapins[3] << (ws - 1 - self.bitcount)
            else:
                self.data |= datapins[3] << self.bitcount

        # Guesstimate the endsample for this bit (can be overridden below).
        es = self.samplenum
        if self.bitcount > 0:
            es += self.samplenum - self.bits[0][1]

        self.bits.insert(0, [datapins[3], self.samplenum, es])

        if self.bitcount > 0:
            self.bits[1][2] = self.samplenum

        self.bitcount += 1

        # Continue to receive if not enough bits were received, yet.
        if self.bitcount != nibws:
            return

        self.putdata()

        self.reset_decoder_state()

    def find_clk_edge(self, datapins, clk, cs, first):
        if self.have_cs and (first or (self.matched & (0b1 << self.have_cs))):
            # Send all CS# pin value changes.
            oldcs = None if first else 1 - cs

            # Reset decoder state when CS# changes (and the CS# pin is used).
            self.reset_decoder_state()

        # We only care about samples if CS# is asserted.
        if self.have_cs and not self.cs_asserted(cs):
            return

        # Ignore sample if the clock pin hasn't changed.
        if first or not (self.matched & (0b1 << 0)):
            return

        # Found the correct clock edge, now get the SPI bit(s).
        self.handle_bit(datapins, clk, cs)

    def decode(self):
        # The CLK & IO0 input is mandatory. Other signals are (individually)
        # optional. Tell stacked decoders when we don't have a CS# signal.
        if not self.has_channel(0):
            raise ChannelError('CLK pin required.')
        self.have_io1 = self.has_channel(2)
        self.have_io3 = self.has_channel(3) & self.has_channel(4)
        self.have_cs = self.has_channel(5)

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
        d = (d3, d2, d1, d0);
        self.find_clk_edge(d, clk, cs, True)

        while True:
            (clk, d0, d1, d2, d3, cs) = self.wait(wait_cond)
            d = (d3, d2, d1, d0);
            self.find_clk_edge(d, clk, cs, False)#
