##
## This file is part of the libsigrokdecode project.
##
## Copyright (C) 2016 Rudolf Reuter <reuterru@arcor.de>
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

class Decoder(srd.Decoder):
    api_version = 3
    id = 'gpib'
    name = 'GPIB'
    longname = 'General Purpose Interface Bus'
    desc = 'IEEE-488 General Purpose Interface Bus (GPIB / HPIB).'
    license = 'gplv2+'
    inputs = ['logic']
    outputs = []
    tags = ['PC']
    channels = (
        {'id': 'dio1' , 'name': 'DIO1', 'desc': 'Data I/O bit 1', 'idn':'dec_gpib_chan_dio1'},
        {'id': 'dio2' , 'name': 'DIO2', 'desc': 'Data I/O bit 2', 'idn':'dec_gpib_chan_dio2'},
        {'id': 'dio3' , 'name': 'DIO3', 'desc': 'Data I/O bit 3', 'idn':'dec_gpib_chan_dio3'},
        {'id': 'dio4' , 'name': 'DIO4', 'desc': 'Data I/O bit 4', 'idn':'dec_gpib_chan_dio4'},
        {'id': 'dio5' , 'name': 'DIO5', 'desc': 'Data I/O bit 5', 'idn':'dec_gpib_chan_dio5'},
        {'id': 'dio6' , 'name': 'DIO6', 'desc': 'Data I/O bit 6', 'idn':'dec_gpib_chan_dio6'},
        {'id': 'dio7' , 'name': 'DIO7', 'desc': 'Data I/O bit 7', 'idn':'dec_gpib_chan_dio7'},
        {'id': 'dio8' , 'name': 'DIO8', 'desc': 'Data I/O bit 8', 'idn':'dec_gpib_chan_dio8'},
        {'id': 'eoi', 'name': 'EOI', 'desc': 'End or identify', 'idn':'dec_gpib_chan_eoi'},
        {'id': 'dav', 'name': 'DAV', 'desc': 'Data valid', 'idn':'dec_gpib_chan_dav'},
        {'id': 'nrfd', 'name': 'NRFD', 'desc': 'Not ready for data', 'idn':'dec_gpib_chan_nrfd'},
        {'id': 'ndac', 'name': 'NDAC', 'desc': 'Not data accepted', 'idn':'dec_gpib_chan_ndac'},
        {'id': 'ifc', 'name': 'IFC', 'desc': 'Interface clear', 'idn':'dec_gpib_chan_ifc'},
        {'id': 'srq', 'name': 'SRQ', 'desc': 'Service request', 'idn':'dec_gpib_chan_srq'},
        {'id': 'atn', 'name': 'ATN', 'desc': 'Attention', 'idn':'dec_gpib_chan_atn'},
        {'id': 'ren', 'name': 'REN', 'desc': 'Remote enable', 'idn':'dec_gpib_chan_ren'},
    )
    options = (
        {'id': 'sample_total', 'desc': 'Total number of samples', 'default': 0, 'idn':'dec_gpib_opt_sample_total'},
    )
    annotations = (
        ('items', 'Items'),
        ('gpib', 'DAT/CMD'),
        ('eoi', 'EOI'),
    )
    annotation_rows = (
        ('bytes', 'Bytes', (0,)),
        ('gpib', 'DAT/CMD', (1,)),
        ('eoi', 'EOI', (2,)),
    )

    def __init__(self):
        self.reset()

    def reset(self):
        self.items = []
        self.itemcount = 0
        self.saved_item = None
        self.saved_ATN = False
        self.saved_EOI = False
        self.samplenum = 0
        self.ss_item = self.es_item = None
        self.first = True

    def start(self):
        self.out_ann = self.register(srd.OUTPUT_ANN)

    def putb(self, data):
        self.put(self.ss_item, self.es_item, self.out_ann, data)

    def handle_bits(self, datapins):
        dbyte = 0x20
        dATN = False
        item2 = False
        dEOI = False
        item3 = False
        # If this is the first item in a word, save its sample number.
        if self.itemcount == 0:
            self.ss_word = self.samplenum

        # Get the bits for this item.
        item = 0
        for i in range(8):
            item |= datapins[i] << i

        item = item ^ 0xff # Invert data byte.
        self.items.append(item)
        self.itemcount += 1

        if datapins[14] == 0:
            item2 = True
        if datapins[8] == 0:
            item3 = True

        if self.first:
            # Save the start sample and item for later (no output yet).
            self.ss_item = self.samplenum
            self.first = False
            self.saved_item = item
            self.saved_ATN = item2
            self.saved_EOI = item3
        else:
            # Output the saved item.
            dbyte = self.saved_item
            dATN = self.saved_ATN
            dEOI = self.saved_EOI
            self.es_item = self.samplenum
            self.putb([0, ['%02X' % self.saved_item]])

            # Encode item byte to GPIB convention.
            self.strgpib = ' '
            if dATN: # ATN, decode commands.
                if dbyte == 0x01: self.strgpib = 'GTL'
                if dbyte == 0x04: self.strgpib = 'SDC'
                if dbyte == 0x05: self.strgpib = 'PPC'
                if dbyte == 0x08: self.strgpib = 'GET'
                if dbyte == 0x09: self.strgpib = 'TCT'
                if dbyte == 0x11: self.strgpib = 'LLO'
                if dbyte == 0x14: self.strgpib = 'DCL'
                if dbyte == 0x15: self.strgpib = 'PPU'
                if dbyte == 0x18: self.strgpib = 'SPE'
                if dbyte == 0x19: self.strgpib = 'SPD'
                if dbyte == 0x3f: self.strgpib = 'UNL'
                if dbyte == 0x5f: self.strgpib = 'UNT'
                if dbyte > 0x1f and dbyte < 0x3f: # Address Listener.
                    self.strgpib = 'L' + chr(dbyte + 0x10)
                if dbyte > 0x3f and dbyte < 0x5f: # Address Talker
                    self.strgpib = 'T' + chr(dbyte - 0x10)
            else:
                if dbyte > 0x1f and dbyte < 0x7f:
                    self.strgpib = chr(dbyte)
                if dbyte == 0x0a:
                    self.strgpib = 'LF'
                if dbyte == 0x0d:
                    self.strgpib = 'CR'

            self.putb([1, [self.strgpib]])
            self.strEOI = ' '
            if dEOI:
                self.strEOI = 'EOI'
            self.putb([2, [self.strEOI]])

            self.ss_item = self.samplenum
            self.saved_item = item
            self.saved_ATN = item2
            self.saved_EOI = item3

        if self.itemcount < 16:
            return

        self.itemcount, self.items = 0, []

    def decode(self):

        # Inspect samples at falling edge of DAV. But make sure to also
        # start inspection when the capture happens to start with low
        # DAV level. Optionally enforce processing when a user specified
        # sample number was reached.
        waitcond = [{9: 'l'}]
        lsn = self.options['sample_total']
        if lsn:
            waitcond.append({'skip': lsn})
        while True:
            if lsn:
                waitcond[1]['skip'] = lsn - self.samplenum - 1
            (d1, d2, d3, d4, d5, d6, d7, d8, eoi, dav, nrfd, ndac, ifc, srq, atn, ren) = self.wait(waitcond)
            pins = (d1, d2, d3, d4, d5, d6, d7, d8, eoi, dav, nrfd, ndac, ifc, srq, atn, ren)
            self.handle_bits(pins)
            waitcond[0][9] = 'f'
