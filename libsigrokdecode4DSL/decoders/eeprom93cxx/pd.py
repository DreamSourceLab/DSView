##
## This file is part of the libsigrokdecode project.
##
## Copyright (C) 2017 Kevin Redon <kingkevin@cuvoodoo.info>
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
    api_version = 2
    id = 'eeprom93cxx'
    name = '93Cxx EEPROM'
    longname = '93Cxx Microwire EEPROM'
    desc = '93Cxx series Microwire EEPROM protocol.'
    license = 'gplv2+'
    inputs = ['microwire']
    outputs = ['eeprom93cxx']
    options = (
        {'id': 'addresssize', 'desc': 'Address size', 'default': 8},
        {'id': 'wordsize', 'desc': 'Word size', 'default': 16},
    )
    annotations = (
        ('si-data', 'SI data'),
        ('so-data', 'SO data'),
        ('warning', 'Warning'),
    )
    annotation_rows = (
        ('data', 'Data', (0, 1)),
        ('warnings', 'Warnings', (2,)),
    )

    def __init__(self):
        self.frame = []

    def start(self):
        self.out_ann = self.register(srd.OUTPUT_ANN)
        self.addresssize = self.options['addresssize']
        self.wordsize = self.options['wordsize']

    def put_address(self, data):
        # Get address (MSb first).
        a = 0
        for b in range(len(data)):
            a += (data[b].si << (len(data) - b - 1))
        self.put(data[0].ss, data[-1].es, self.out_ann,
                 [0, ['Address: 0x%x' % a, 'Addr: 0x%x' % a, '0x%x' % a]])

    def put_word(self, si, data):
        # Decode word (MSb first).
        word = 0
        for b in range(len(data)):
            d = data[b].si if si else data[b].so
            word += (d << (len(data) - b - 1))
        idx = 0 if si else 1
        self.put(data[0].ss, data[-1].es,
                 self.out_ann, [idx, ['Data: 0x%x' % word, '0x%x' % word]])

    def decode(self, ss, es, data):
        if len(data) < (2 + self.addresssize):
            self.put(ss, es, self.out_ann, [2, ['Not enough packet bits']])
            return

        opcode = (data[0].si << 1) + (data[1].si << 0)

        if opcode == 2:
            # READ instruction.
            self.put(data[0].ss, data[1].es,
                     self.out_ann, [0, ['Read word', 'READ']])
            self.put_address(data[2:2 + self.addresssize])

            # Get all words.
            word_start = 2 + self.addresssize
            while len(data) - word_start > 0:
                # Check if there are enough bits for a word.
                if len(data) - word_start < self.wordsize:
                    self.put(data[word_start].ss, data[len(data) - 1].es,
                             self.out_ann, [2, ['Not enough word bits']])
                    break
                self.put_word(False, data[word_start:word_start + self.wordsize])
                # Go to next word.
                word_start += self.wordsize
        elif opcode == 1:
            # WRITE instruction.
            self.put(data[0].ss, data[1].es,
                     self.out_ann, [0, ['Write word', 'WRITE']])
            self.put_address(data[2:2 + self.addresssize])
            # Get word.
            if len(data) < 2 + self.addresssize + self.wordsize:
                self.put(data[2 + self.addresssize].ss,
                         data[len(data) - 1].ss,
                         self.out_ann, [2, ['Not enough word bits']])
            else:
                self.put_word(True, data[2 + self.addresssize:2 + self.addresssize + self.wordsize])
        elif opcode == 3:
            # ERASE instruction.
            self.put(data[0].ss, data[1].es,
                     self.out_ann, [0, ['Erase word', 'ERASE']])
            self.put_address(data[2:2 + self.addresssize])
        elif opcode == 0:
            if data[2].si == 1 and data[3].si == 1:
                # WEN instruction.
                self.put(data[0].ss, data[2 + self.addresssize - 1].es,
                         self.out_ann, [0, ['Write enable', 'WEN']])
            elif data[2].si == 0 and data[3].si == 0:
                # WDS instruction.
                self.put(data[0].ss, data[2 + self.addresssize - 1].es,
                         self.out_ann, [0, ['Write disable', 'WDS']])
            elif data[2].si == 1 and data[3].si == 0:
                # ERAL instruction.
                self.put(data[0].ss, data[2 + self.addresssize - 1].es,
                         self.out_ann, [0, ['Erase all memory',
                                            'Erase all', 'ERAL']])
            elif data[2].si == 0 and data[3].si == 1:
                # WRAL instruction.
                self.put(data[0].ss, data[2 + self.addresssize - 1].es,
                         self.out_ann, [0, ['Write all memory',
                                            'Write all', 'WRAL']])
                # Get word.
                if len(data) < 2 + self.addresssize + self.wordsize:
                    self.put(data[2 + self.addresssize].ss,
                             data[len(data) - 1].ss,
                             self.out_ann, [2, ['Not enough word bits']])
                else:
                    self.put_word(True, data[2 + self.addresssize:2 + self.addresssize + self.wordsize])
