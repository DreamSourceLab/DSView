##
## This file is part of the libsigrokdecode project.
##
## Copyright (C) 2012 Iztok Jeras <iztok.jeras@gmail.com>
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

class SamplerateError(Exception):
    pass

class Decoder(srd.Decoder):
    api_version = 2
    id = 'onewire_link'
    name = 'One-Wire link layer'
    longname = 'One-Wire serial communication bus (link layer)'
    desc = 'Bidirectional, half-duplex, asynchronous serial bus.'
    license = 'gplv2+'
    inputs = ['logic']
    outputs = ['onewire_link']
    channels = (
        {'id': 'owr', 'name': 'OWR', 'desc': '1-Wire signal line'},
    )
    optional_channels = (
        {'id': 'pwr', 'name': 'PWR', 'desc': '1-Wire power supply pin'},
    )
    options = (
        {'id': 'overdrive',
            'desc': 'Overdrive mode', 'default': 'no', 'values': ('yes', 'no')},
        # Time options (specified in microseconds):
        {'id': 'cnt_normal_bit',
            'desc': 'Normal mode sample bit time (μs)', 'default': 15},
        {'id': 'cnt_normal_slot',
            'desc': 'Normal mode data slot time (μs)', 'default': 60},
        {'id': 'cnt_normal_presence',
            'desc': 'Normal mode sample presence time (μs)', 'default': 75},
        {'id': 'cnt_normal_reset',
            'desc': 'Normal mode reset time (μs)', 'default': 480},
        {'id': 'cnt_overdrive_bit',
            'desc': 'Overdrive mode sample bit time (μs)', 'default': 2},
        {'id': 'cnt_overdrive_slot',
            'desc': 'Overdrive mode data slot time (μs)', 'default': 7.3},
        {'id': 'cnt_overdrive_presence',
            'desc': 'Overdrive mode sample presence time (μs)', 'default': 10},
        {'id': 'cnt_overdrive_reset',
            'desc': 'Overdrive mode reset time (μs)', 'default': 48},
    )
    annotations = (
        ('bit', 'Bit'),
        ('warnings', 'Warnings'),
        ('reset', 'Reset'),
        ('presence', 'Presence'),
        ('overdrive', 'Overdrive mode notifications'),
    )
    annotation_rows = (
        ('bits', 'Bits', (0, 2, 3)),
        ('info', 'Info', (4,)),
        ('warnings', 'Warnings', (1,)),
    )

    def putm(self, data):
        self.put(0, 0, self.out_ann, data)

    def putpb(self, data):
        self.put(self.fall, self.samplenum, self.out_python, data)

    def putb(self, data):
        self.put(self.fall, self.samplenum, self.out_ann, data)

    def putx(self, data):
        self.put(self.fall, self.cnt_bit[self.overdrive], self.out_ann, data)

    def putfr(self, data):
        self.put(self.fall, self.rise, self.out_ann, data)

    def putprs(self, data):
        self.put(self.rise, self.samplenum, self.out_python, data)

    def putrs(self, data):
        self.put(self.rise, self.samplenum, self.out_ann, data)

    def __init__(self):
        self.samplerate = None
        self.samplenum = 0
        self.state = 'WAIT FOR FALLING EDGE'
        self.present = 0
        self.bit = 0
        self.bit_cnt = 0
        self.command = 0
        self.overdrive = 0
        self.fall = 0
        self.rise = 0

    def start(self):
        self.out_python = self.register(srd.OUTPUT_PYTHON)
        self.out_ann = self.register(srd.OUTPUT_ANN)

    def checks(self):
        # Check if samplerate is appropriate.
        if self.options['overdrive'] == 'yes':
            if self.samplerate < 2000000:
                self.putm([1, ['Sampling rate is too low. Must be above ' +
                               '2MHz for proper overdrive mode decoding.']])
            elif self.samplerate < 5000000:
                self.putm([1, ['Sampling rate is suggested to be above 5MHz ' +
                               'for proper overdrive mode decoding.']])
        else:
            if self.samplerate < 400000:
                self.putm([1, ['Sampling rate is too low. Must be above ' +
                               '400kHz for proper normal mode decoding.']])
            elif self.samplerate < 1000000:
                self.putm([1, ['Sampling rate is suggested to be above ' +
                               '1MHz for proper normal mode decoding.']])

        # Check if sample times are in the allowed range.

        time_min = float(self.cnt_normal_bit) / self.samplerate
        time_max = float(self.cnt_normal_bit + 1) / self.samplerate
        if (time_min < 0.000005) or (time_max > 0.000015):
            self.putm([1, ['The normal mode data sample time interval ' +
                 '(%2.1fus-%2.1fus) should be inside (5.0us, 15.0us).'
                 % (time_min * 1000000, time_max * 1000000)]])

        time_min = float(self.cnt_normal_presence) / self.samplerate
        time_max = float(self.cnt_normal_presence + 1) / self.samplerate
        if (time_min < 0.0000681) or (time_max > 0.000075):
            self.putm([1, ['The normal mode presence sample time interval ' +
                 '(%2.1fus-%2.1fus) should be inside (68.1us, 75.0us).'
                 % (time_min * 1000000, time_max * 1000000)]])

        time_min = float(self.cnt_overdrive_bit) / self.samplerate
        time_max = float(self.cnt_overdrive_bit + 1) / self.samplerate
        if (time_min < 0.000001) or (time_max > 0.000002):
            self.putm([1, ['The overdrive mode data sample time interval ' +
                 '(%2.1fus-%2.1fus) should be inside (1.0us, 2.0us).'
                 % (time_min * 1000000, time_max * 1000000)]])

        time_min = float(self.cnt_overdrive_presence) / self.samplerate
        time_max = float(self.cnt_overdrive_presence + 1) / self.samplerate
        if (time_min < 0.0000073) or (time_max > 0.000010):
            self.putm([1, ['The overdrive mode presence sample time interval ' +
                 '(%2.1fus-%2.1fus) should be inside (7.3us, 10.0us).'
                 % (time_min * 1000000, time_max * 1000000)]])


    def metadata(self, key, value):
        if key != srd.SRD_CONF_SAMPLERATE:
            return
        self.samplerate = value

        # The default 1-Wire time base is 30us. This is used to calculate
        # sampling times.
        samplerate = float(self.samplerate)

        x = float(self.options['cnt_normal_bit']) / 1000000.0
        self.cnt_normal_bit = int(samplerate * x) - 1
        x = float(self.options['cnt_normal_slot']) / 1000000.0
        self.cnt_normal_slot = int(samplerate * x) - 1
        x = float(self.options['cnt_normal_presence']) / 1000000.0
        self.cnt_normal_presence = int(samplerate * x) - 1
        x = float(self.options['cnt_normal_reset']) / 1000000.0
        self.cnt_normal_reset = int(samplerate * x) - 1
        x = float(self.options['cnt_overdrive_bit']) / 1000000.0
        self.cnt_overdrive_bit = int(samplerate * x) - 1
        x = float(self.options['cnt_overdrive_slot']) / 1000000.0
        self.cnt_overdrive_slot = int(samplerate * x) - 1
        x = float(self.options['cnt_overdrive_presence']) / 1000000.0
        self.cnt_overdrive_presence = int(samplerate * x) - 1
        x = float(self.options['cnt_overdrive_reset']) / 1000000.0
        self.cnt_overdrive_reset = int(samplerate * x) - 1

        # Organize values into lists.
        self.cnt_bit = [self.cnt_normal_bit, self.cnt_overdrive_bit]
        self.cnt_presence = [self.cnt_normal_presence, self.cnt_overdrive_presence]
        self.cnt_reset = [self.cnt_normal_reset, self.cnt_overdrive_reset]
        self.cnt_slot = [self.cnt_normal_slot, self.cnt_overdrive_slot]

    def decode(self, ss, es, data):
        if not self.samplerate:
            raise SamplerateError('Cannot decode without samplerate.')
        for (self.samplenum, (owr, pwr)) in data:
            data.itercnt += 1
            if self.samplenum == 0:
                self.checks()
            # State machine.
            if self.state == 'WAIT FOR FALLING EDGE':
                # The start of a cycle is a falling edge.
                if owr != 0:
                    continue
                # Save the sample number for the falling edge.
                self.fall = self.samplenum
                # Go to waiting for sample time.
                self.state = 'WAIT FOR DATA SAMPLE'
            elif self.state == 'WAIT FOR DATA SAMPLE':
                # Sample data bit.
                t = self.samplenum - self.fall
                if t == self.cnt_bit[self.overdrive]:
                    self.bit = owr
                    self.state = 'WAIT FOR DATA SLOT END'
            elif self.state == 'WAIT FOR DATA SLOT END':
                # A data slot ends in a recovery period, otherwise, this is
                # probably a reset.
                t = self.samplenum - self.fall
                if t != self.cnt_slot[self.overdrive]:
                    continue

                if owr == 0:
                    # This seems to be a reset slot, wait for its end.
                    self.state = 'WAIT FOR RISING EDGE'
                    continue

                self.putb([0, ['Bit: %d' % self.bit, '%d' % self.bit]])
                self.putpb(['BIT', self.bit])

                # Checking the first command to see if overdrive mode
                # should be entered.
                if self.bit_cnt <= 8:
                    self.command |= (self.bit << self.bit_cnt)
                elif self.bit_cnt == 8 and self.command in [0x3c, 0x69]:
                    self.putx([4, ['Entering overdrive mode', 'Overdrive on']])
                # Increment the bit counter.
                self.bit_cnt += 1
                # Wait for next slot.
                self.state = 'WAIT FOR FALLING EDGE'
            elif self.state == 'WAIT FOR RISING EDGE':
                # The end of a cycle is a rising edge.
                if owr != 1:
                    continue

                # Check if this was a reset cycle.
                t = self.samplenum - self.fall
                if t > self.cnt_normal_reset:
                    # Save the sample number for the rising edge.
                    self.rise = self.samplenum
                    self.putfr([2, ['Reset', 'Rst', 'R']])
                    self.state = 'WAIT FOR PRESENCE DETECT'
                    # Exit overdrive mode.
                    if self.overdrive:
                        self.putx([4, ['Exiting overdrive mode', 'Overdrive off']])
                        self.overdrive = 0
                    # Clear command bit counter and data register.
                    self.bit_cnt = 0
                    self.command = 0
                elif (t > self.cnt_overdrive_reset) and self.overdrive:
                    # Save the sample number for the rising edge.
                    self.rise = self.samplenum
                    self.putfr([2, ['Reset', 'Rst', 'R']])
                    self.state = 'WAIT FOR PRESENCE DETECT'
                # Otherwise this is assumed to be a data bit.
                else:
                    self.state = 'WAIT FOR FALLING EDGE'
            elif self.state == 'WAIT FOR PRESENCE DETECT':
                # Sample presence status.
                t = self.samplenum - self.rise
                if t == self.cnt_presence[self.overdrive]:
                    self.present = owr
                    self.state = 'WAIT FOR RESET SLOT END'
            elif self.state == 'WAIT FOR RESET SLOT END':
                # A reset slot ends in a long recovery period.
                t = self.samplenum - self.rise
                if t != self.cnt_reset[self.overdrive]:
                    continue

                if owr == 0:
                    # This seems to be a reset slot, wait for its end.
                    self.state = 'WAIT FOR RISING EDGE'
                    continue

                p = 'false' if self.present else 'true'
                self.putrs([3, ['Presence: %s' % p, 'Presence', 'Pres', 'P']])
                self.putprs(['RESET/PRESENCE', not self.present])

                # Wait for next slot.
                self.state = 'WAIT FOR FALLING EDGE'
