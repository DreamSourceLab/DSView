##
## This file is part of the libsigrokdecode project.
##
## Copyright (C) 2016 Rudolf Reuter <reuterru@arcor.de>
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

class Decoder(srd.Decoder):
    api_version = 2
    id = 'gpib'
    name = 'GPIB'
    longname = 'General Purpose Interface Bus'
    desc = 'IEEE-488 GPIB / HPIB protocol.'
    license = 'gplv2+'
    inputs = ['logic']
    outputs = ['gpib']
    channels = (
        {'id': 'dio1' , 'name': 'DIO1', 'desc': 'Data I/O bit 1'},
        {'id': 'dio2' , 'name': 'DIO2', 'desc': 'Data I/O bit 2'},
        {'id': 'dio3' , 'name': 'DIO3', 'desc': 'Data I/O bit 3'},
        {'id': 'dio4' , 'name': 'DIO4', 'desc': 'Data I/O bit 4'},
        {'id': 'dio5' , 'name': 'DIO5', 'desc': 'Data I/O bit 5'},
        {'id': 'dio6' , 'name': 'DIO6', 'desc': 'Data I/O bit 6'},
        {'id': 'dio7' , 'name': 'DIO7', 'desc': 'Data I/O bit 7'},
        {'id': 'dio8' , 'name': 'DIO8', 'desc': 'Data I/O bit 8'},
        {'id': 'eoi', 'name': 'EOI', 'desc': 'End or identify'},
        {'id': 'dav', 'name': 'DAV', 'desc': 'Data valid'},
        {'id': 'nrfd', 'name': 'NRFD', 'desc': 'Not ready for data'},
        {'id': 'ndac', 'name': 'NDAC', 'desc': 'Not data accepted'},
        {'id': 'ifc', 'name': 'IFC', 'desc': 'Interface clear'},
        {'id': 'srq', 'name': 'SRQ', 'desc': 'Service request'},
        {'id': 'atn', 'name': 'ATN', 'desc': 'Attention'},
        {'id': 'ren', 'name': 'REN', 'desc': 'Remote enable'},
    )
    options = (
        {'id': 'sample_total', 'desc': 'Total number of samples', 'default': 0},
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
        self.olddav = None
        self.items = []
        self.itemcount = 0
        self.saved_item = None
        self.saved_ATN = False
        self.saved_EOI = False
        self.samplenum = 0
        self.oldpins = None
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

    def find_falling_dav_edge(self, dav, datapins):
        # Ignore sample if the DAV pin hasn't changed.
        if dav == self.olddav:
            return
        self.olddav = dav
        # Sample on falling DAV edge.
        if dav == 1:
            return

        # Found the correct DAV edge, now get the bits.
        self.handle_bits(datapins)

    def decode(self, ss, es, data):
        lsn = self.options['sample_total']
        data.itercnt += 1
        for (self.samplenum, pins) in data:
            if lsn > 0:
                if (lsn - self.samplenum) == 1: # Show the last data word.
                    self.handle_bits(pins)

            # Ignore identical samples early on (for performance reasons).
            if self.oldpins == pins:
                continue
            self.oldpins = pins

            self.find_falling_dav_edge(pins[9], pins)
