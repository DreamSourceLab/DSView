##
## This file is part of the libsigrokdecode project.
##
## Copyright (C) 2014 Torsten Duwe <duwe@suse.de>
## Copyright (C) 2014 Sebastien Bourdelin <sebastien.bourdelin@savoirfairelinux.com>
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
from collections import deque

class SamplerateError(Exception):
    pass

def normalize_time(t):
    if t >= 1.0:
        return '%.3f s  (%.3f Hz)' % (t, (1/t))
    elif t >= 0.001:
        if 1/t/1000 < 1:
            return '%.3f ms (%.3f Hz)' % (t * 1000.0, (1/t))
        else:
            return '%.3f ms (%.3f kHz)' % (t * 1000.0, (1/t)/1000)
    elif t >= 0.000001:
        if 1/t/1000/1000 < 1:
            return '%.3f μs (%.3f kHz)' % (t * 1000.0 * 1000.0, (1/t)/1000)
        else:
            return '%.3f μs (%.3f MHz)' % (t * 1000.0 * 1000.0, (1/t)/1000/1000)
    elif t >= 0.000000001:
        if 1/t/1000/1000/1000:
            return '%.3f ns (%.3f MHz)' % (t * 1000.0 * 1000.0 * 1000.0, (1/t)/1000/1000)
        else:
            return '%.3f ns (%.3f GHz)' % (t * 1000.0 * 1000.0 * 1000.0, (1/t)/1000/1000/1000)
    else:
        return '%f' % t

class Decoder(srd.Decoder):
    api_version = 2
    id = 'timing'
    name = 'Timing'
    longname = 'Timing calculation with frequency and averaging'
    desc = 'Calculate time between edges.'
    license = 'gplv2+'
    inputs = ['logic']
    outputs = ['timing']
    channels = (
        {'id': 'data', 'name': 'Data', 'desc': 'Data line'},
    )
    annotations = (
        ('time', 'Time'),
        ('average', 'Average'),
    )
    annotation_rows = (
        ('time', 'Time', (0,)),
        ('average', 'Average', (1,)),
    )
    options = (
        { 'id': 'avg_period', 'desc': 'Averaging period', 'default': 100 },
    )

    def __init__(self):
        self.samplerate = None
        self.oldpin = None
        self.last_samplenum = None
        self.last_n = deque()
        self.chunks = 0

    def metadata(self, key, value):
        if key == srd.SRD_CONF_SAMPLERATE:
            self.samplerate = value

    def start(self):
        self.out_ann = self.register(srd.OUTPUT_ANN)

    def decode(self, ss, es, data):
        if not self.samplerate:
            raise SamplerateError('Cannot decode without samplerate.')

        for (self.samplenum, (pin,)) in data:
            data.itercnt += 1
            if self.oldpin is None:
                self.oldpin = pin
                self.last_samplenum = self.samplenum
                continue

            if self.oldpin != pin:
                samples = self.samplenum - self.last_samplenum
                t = samples / self.samplerate
                self.chunks += 1

                # Don't insert the first chunk into the averaging as it is
                # not complete probably.
                if self.last_samplenum is None or self.chunks < 2:
                    # Report the timing normalized.
                    self.put(self.last_samplenum, self.samplenum, self.out_ann,
                             [0, [normalize_time(t)]])
                else:
                    if t > 0:
                        self.last_n.append(t)

                    if len(self.last_n) > self.options['avg_period']:
                        self.last_n.popleft()

                    # Report the timing normalized.
                    self.put(self.last_samplenum, self.samplenum, self.out_ann,
                             [0, [normalize_time(t)]])
                    self.put(self.last_samplenum, self.samplenum, self.out_ann,
                             [1, [normalize_time(sum(self.last_n) / len(self.last_n))]])

                # Store data for next round.
                self.last_samplenum = self.samplenum
                self.oldpin = pin
