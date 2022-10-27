##
## This file is part of the libsigrokdecode project.
##
## Copyright (C) 2014 Gump Yang <gump.yang@gmail.com>
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
from .lists import *

class SamplerateError(Exception):
    pass

class Decoder(srd.Decoder):
    api_version = 3
    id = 'ir_nec'
    name = 'IR NEC'
    longname = 'IR NEC'
    desc = 'NEC infrared remote control protocol.'
    license = 'gplv2+'
    inputs = ['logic']
    outputs = []
    tags = ['IR']
    channels = (
        {'id': 'ir', 'name': 'IR', 'desc': 'Data line', 'idn':'dec_ir_nec_chan_ir'},
    )
    options = (
        {'id': 'polarity', 'desc': 'Polarity', 'default': 'active-low',
            'values': ('active-low', 'active-high'), 'idn':'dec_ir_nec_opt_polarity'},
        {'id': 'cd_freq', 'desc': 'Carrier Frequency', 'default': 0, 'idn':'dec_ir_nec_opt_cd_freq'},
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
        self.reset()

    def reset(self):
        self.state = 'IDLE'
        self.ss_bit = self.ss_start = self.ss_other_edge = self.ss_remote = 0
        self.data = self.count = self.active = None
        self.addr = self.cmd = None

    def start(self):
        self.out_ann = self.register(srd.OUTPUT_ANN)
        self.active = 0 if self.options['polarity'] == 'active-low' else 1

    def metadata(self, key, value):
        if key == srd.SRD_CONF_SAMPLERATE:
            self.samplerate = value
        self.tolerance = 0.05 # +/-5%
        self.lc = int(self.samplerate * 0.0135) - 1 # 13.5ms
        self.rc = int(self.samplerate * 0.01125) - 1 # 11.25ms
        self.dazero = int(self.samplerate * 0.001125) - 1 # 1.125ms
        self.daone = int(self.samplerate * 0.00225) - 1 # 2.25ms
        self.stop = int(self.samplerate * 0.000652) - 1 # 0.652ms

    def compare_with_tolerance(self, measured, base):
        return (measured >= base * (1 - self.tolerance)
                and measured <= base * (1 + self.tolerance))

    def handle_bit(self, tick):
        ret = None
        if self.compare_with_tolerance(tick, self.dazero):
            ret = 0
        elif self.compare_with_tolerance(tick, self.daone):
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

    def decode(self):
        if not self.samplerate:
            raise SamplerateError('Cannot decode without samplerate.')

        cd_count = None
        if self.options['cd_freq']:
            cd_count = int(self.samplerate / self.options['cd_freq']) + 1
            prev_ir = None

        while True:
            # Detect changes in the presence of an active input signal.
            # The decoder can either be fed an already filtered RX signal
            # or optionally can detect the presence of a carrier. Periods
            # of inactivity (signal changes slower than the carrier freq,
            # if specified) pass on the most recently sampled level. This
            # approach works for filtered and unfiltered input alike, and
            # only slightly extends the active phase of input signals with
            # carriers included by one period of the carrier frequency.
            # IR based communication protocols can cope with this slight
            # inaccuracy just fine by design. Enabling carrier detection
            # on already filtered signals will keep the length of their
            # active period, but will shift their signal changes by one
            # carrier period before they get passed to decoding logic.
            if cd_count:
                (cur_ir,) = self.wait([{0: 'e'}, {'skip': cd_count}])
                if (self.matched & (0b1 << 0)):
                    cur_ir = self.active
                if cur_ir == prev_ir:
                    continue
                prev_ir = cur_ir
                self.ir = cur_ir
            else:
                (self.ir,) = self.wait({0: 'e'})

            if self.ir != self.active:
                # Save the non-active edge, then wait for the next edge.
                self.ss_other_edge = self.samplenum
                continue

            b = self.samplenum - self.ss_bit

            # State machine.
            if self.state == 'IDLE':
                if self.compare_with_tolerance(b, self.lc):
                    self.putpause('Long')
                    self.putx([5, ['Leader code', 'Leader', 'LC', 'L']])
                    self.ss_remote = self.ss_start
                    self.data = self.count = 0
                    self.state = 'ADDRESS'
                elif self.compare_with_tolerance(b, self.rc):
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
