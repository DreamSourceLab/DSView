##
## This file is part of the libsigrokdecode project.
##
## Copyright (C) 2018 fenugrec <fenugrec@users.sourceforge.net>
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

class ChannelError(Exception):
    pass

class Decoder(srd.Decoder):
    api_version = 3
    id = 'mcs48'
    name = 'MCS-48'
    longname = 'Intel MCS-48'
    desc = 'Intel MCS-48 external memory access protocol.'
    license = 'gplv2+'
    inputs = ['logic']
    outputs = []
    tags = ['Retro computing']
    channels = (
        {'id': 'ale', 'name': 'ALE', 'desc': 'Address latch enable', 'idn':'dec_mcs48_chan_ale'},
        {'id': 'psen', 'name': '/PSEN', 'desc': 'Program store enable', 'idn':'dec_mcs48_chan_psen'},
    ) + tuple({
        'id': 'd%d' % i,
        'name': 'D%d' % i,
        'desc': 'CPU data line %d' % i
        #, 'idn':'dec_mcs48_chan_d%d' % i
        } for i in range(0, 8)
    ) + tuple({
        'id': 'a%d' % i,
        'name': 'A%d' % i,
        'desc': 'CPU address line %d' % i
        #, 'idn':'dec_mcs48_chan_a%d' % i
        } for i in range(8, 12)
    )
    optional_channels = tuple({
        'id': 'a%d' % i,
        'name': 'A%d' % i,
        'desc': 'CPU address line %d' % i
        #, 'idn':'dec_mcs48_opt_chan_a%d' % i
        } for i in range(12, 13)
    )
    annotations = (
        ('romdata', 'Address:Data'),
    )
    binary = (
        ('romdata', 'AAAA:DD'),
    )

    def __init__(self):
        self.reset()

    def reset(self):
        self.addr = 0
        self.addr_s = 0
        self.data = 0
        self.data_s = 0

        # Flag to make sure we get an ALE pulse first.
        self.started = 0

    def start(self):
        self.out_ann = self.register(srd.OUTPUT_ANN)
        self.out_bin = self.register(srd.OUTPUT_BINARY)

    def newaddr(self, addr, data):
        # Falling edge on ALE: reconstruct address.
        self.started = 1
        addr = sum([bit << i for i, bit in enumerate(addr)])
        addr <<= len(data)
        addr |= sum([bit << i for i, bit in enumerate(data)])
        self.addr = addr
        self.addr_s = self.samplenum

    def newdata(self, data):
        # Edge on PSEN: get data.
        data = sum([bit << i for i, bit in enumerate(data)])
        self.data = data
        self.data_s = self.samplenum
        if self.started:
            anntext = '{:04X}:{:02X}'.format(self.addr, self.data)
            self.put(self.addr_s, self.data_s, self.out_ann, [0, [anntext]])
            bindata = self.addr.to_bytes(2, byteorder='big')
            bindata += self.data.to_bytes(1, byteorder='big')
            self.put(self.addr_s, self.data_s, self.out_bin, [0, bindata])

    def decode(self):
        # Address bits above A11 are optional, and are considered to be A12+.
        # This logic needs more adjustment when more bank address pins are
        # to get supported. For now, having just A12 is considered sufficient.
        has_bank = self.has_channel(14)
        bank_pin_count = 1 if has_bank else 0
        # Sample address on the falling ALE edge.
        # Save data on falling edge of PSEN.
        while True:
            (ale, psen, d0, d1, d2, d3, d4, d5, d6, d7, a8, a9, a10, a11, a12) = self.wait([{0: 'f'}, {1: 'r'}])
            data = (d0, d1, d2, d3, d4, d5, d6, d7)
            addr = (a8, a9, a10, a11)
            bank = (a12, )
            if has_bank:
                addr += bank[:bank_pin_count]
            # Handle those conditions (one or more) that matched this time.
            if (self.matched & (0b1 << 0)):
                self.newaddr(addr, data)
            if (self.matched & (0b1 << 1)):
                self.newdata(data)
