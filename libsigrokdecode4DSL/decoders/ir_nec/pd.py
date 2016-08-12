##
## This file is part of the libsigrokdecode project.
##
## Copyright (C) 2014 Gump Yang <gump.yang@gmail.com>
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
from .lists import *

class SamplerateError(Exception):
    pass

class Decoder(srd.Decoder):
    api_version = 2
    id = 'ir_nec'
    name = 'IR NEC'
    longname = 'IR NEC'
    desc = 'NEC infrared remote control protocol.'
    license = 'gplv2+'
    inputs = ['logic']
    outputs = ['ir_nec']
    channels = (
        {'id': 'ir', 'name': 'IR', 'desc': 'Data line'},
    )
    options = (
        {'id': 'polarity', 'desc': 'Polarity', 'default': 'active-low',
            'values': ('active-low', 'active-high')},
    )
    annotations = (
        ('bit', 'Bit'),
        ('agc-pulse', 'AGC pulse'),
        ('longpause', 'Long pause'),
        ('shortpause', 'Short pause'),
        ('stop-bit', 'Stop bit'),
        ('leader-code', 'Leader code'),
        ('addr', 'Address'),
        ('addr-inv', 'Address#'),
        ('cmd', 'Command'),
        ('cmd-inv', 'Command#'),
        ('repeat-code', 'Repeat code'),
        ('remote', 'Remote'),
        ('warnings', 'Warnings'),
    )
    annotation_rows = (
        ('bits', 'Bits', (0, 1, 2, 3, 4)),
        ('fields', 'Fields', (5, 6, 7, 8, 9, 10)),
        ('remote', 'Remote', (11,)),
        ('warnings', 'Warnings', (12,)),
    )

    def putx(self, data):
        self.put(self.ss_start, self.samplenum, self.out_ann, data)

    def putb(self, data):
        self.put(self.ss_bit, self.samplenum, self.out_ann, data)

    def putd(self, data):
        name = self.state.title()
        d = {'ADDRESS': 6, 'ADDRESS#': 7, 'COMMAND': 8, 'COMMAND#': 9}
        s = {'ADDRESS': ['ADDR', 'A'], 'ADDRESS#': ['ADDR#', 'A#'],
             'COMMAND': ['CMD', 'C'], 'COMMAND#': ['CMD#', 'C#']}
        self.putx([d[self.state], ['%s: 0x%02X' % (name, data),
                  '%s: 0x%02X' % (s[self.state][0], data),
                  '%s: 0x%02X' % (s[self.state][1], data), s[self.state][1]]])

    def putstop(self, ss):
        self.put(ss, ss + self.stop, self.out_ann,
                 [4, ['Stop bit', 'Stop', 'St', 'S']])

    def putpause(self, p):
        self.put(self.ss_start, self.ss_other_edge, self.out_ann,
                 [1, ['AGC pulse', 'AGC', 'A']])
        idx = 2 if p == 'Long' else 3
        self.put(self.ss_other_edge, self.samplenum, self.out_ann,
                 [idx, [p + ' pause', '%s-pause' % p[0], '%sP' % p[0], 'P']])

    def putremote(self):
        dev = address.get(self.addr, 'Unknown device')
        buttons = command.get(self.addr, None)
        if buttons is None:
            btn = ['Unknown', 'Unk']
        else:
            btn = buttons.get(self.cmd, ['Unknown', 'Unk'])
        self.put(self.ss_remote, self.ss_bit + self.stop, self.out_ann,
                 [11, ['%s: %s' % (dev, btn[0]), '%s: %s' % (dev, btn[1]),
                 '%s' % btn[1]]])

    def __init__(self):
        self.state = 'IDLE'
        self.ss_bit = self.ss_start = self.ss_other_edge = self.ss_remote = 0
        self.data = self.count = self.active = self.old_ir = None
        self.addr = self.cmd = None

    def start(self):
        self.out_ann = self.register(srd.OUTPUT_ANN)
        self.active = 0 if self.options['polarity'] == 'active-low' else 1
        self.old_ir = 1 if self.active == 0 else 0

    def metadata(self, key, value):
        if key == srd.SRD_CONF_SAMPLERATE:
            self.samplerate = value
        self.margin = int(self.samplerate * 0.0001) - 1 # 0.1ms
        self.lc = int(self.samplerate * 0.0135) - 1 # 13.5ms
        self.rc = int(self.samplerate * 0.01125) - 1 # 11.25ms
        self.dazero = int(self.samplerate * 0.001125) - 1 # 1.125ms
        self.daone = int(self.samplerate * 0.00225) - 1 # 2.25ms
        self.stop = int(self.samplerate * 0.000652) - 1 # 0.652ms

    def handle_bit(self, tick):
        ret = None
        if tick in range(self.dazero - self.margin, self.dazero + self.margin):
            ret = 0
        elif tick in range(self.daone - self.margin, self.daone + self.margin):
            ret = 1
        if ret in (0, 1):
            self.putb([0, ['%d' % ret]])
            self.data |= (ret << self.count) # LSB-first
            self.count = self.count + 1
        self.ss_bit = self.samplenum

    def data_ok(self):
        ret, name = (self.data >> 8) & (self.data & 0xff), self.state.title()
        if self.count == 8:
            if self.state == 'ADDRESS':
                self.addr = self.data
            if self.state == 'COMMAND':
                self.cmd = self.data
            self.putd(self.data)
            self.ss_start = self.samplenum
            return True
        if ret == 0:
            self.putd(self.data >> 8)
        else:
            self.putx([12, ['%s error: 0x%04X' % (name, self.data)]])
        self.data = self.count = 0
        self.ss_bit = self.ss_start = self.samplenum
        return ret == 0

    def decode(self, ss, es, data):
        if not self.samplerate:
            raise SamplerateError('Cannot decode without samplerate.')
        for (self.samplenum, pins) in data:
            self.ir = pins[0]
            data.itercnt += 1

            # Wait for an "interesting" edge, but also record the other ones.
            if self.old_ir == self.ir:
                continue
            if self.ir != self.active:
                self.ss_other_edge = self.samplenum
                self.old_ir = self.ir
                continue

            b = self.samplenum - self.ss_bit

            # State machine.
            if self.state == 'IDLE':
                if b in range(self.lc - self.margin, self.lc + self.margin):
                    self.putpause('Long')
                    self.putx([5, ['Leader code', 'Leader', 'LC', 'L']])
                    self.ss_remote = self.ss_start
                    self.data = self.count = 0
                    self.state = 'ADDRESS'
                elif b in range(self.rc - self.margin, self.rc + self.margin):
                    self.putpause('Short')
                    self.putstop(self.samplenum)
                    self.samplenum += self.stop
                    self.putx([10, ['Repeat code', 'Repeat', 'RC', 'R']])
                    self.data = self.count = 0
                self.ss_bit = self.ss_start = self.samplenum
            elif self.state == 'ADDRESS':
                self.handle_bit(b)
                if self.count == 8:
                    self.state = 'ADDRESS#' if self.data_ok() else 'IDLE'
            elif self.state == 'ADDRESS#':
                self.handle_bit(b)
                if self.count == 16:
                    self.state = 'COMMAND' if self.data_ok() else 'IDLE'
            elif self.state == 'COMMAND':
                self.handle_bit(b)
                if self.count == 8:
                    self.state = 'COMMAND#' if self.data_ok() else 'IDLE'
            elif self.state == 'COMMAND#':
                self.handle_bit(b)
                if self.count == 16:
                    self.state = 'STOP' if self.data_ok() else 'IDLE'
            elif self.state == 'STOP':
                self.putstop(self.ss_bit)
                self.putremote()
                self.ss_bit = self.ss_start = self.samplenum
                self.state = 'IDLE'

            self.old_ir = self.ir
