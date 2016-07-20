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

class SamplerateError(Exception):
    pass

def normalize_time(t):
    if t >= 1.0:
        return '%.3f s' % t
    elif t >= 0.001:
        return '%.3f ms' % (t * 1000.0)
    elif t >= 0.000001:
        return '%.3f μs' % (t * 1000.0 * 1000.0)
    elif t >= 0.000000001:
        return '%.3f ns' % (t * 1000.0 * 1000.0 * 1000.0)
    else:
        return '%f' % t

class Decoder(srd.Decoder):
    api_version = 2
    id = 'timing'
    name = 'Timing'
    longname = 'Timing calculation'
    desc = 'Calculate time between edges.'
    license = 'gplv2+'
    inputs = ['logic']
    outputs = ['timing']
    channels = (
        {'id': 'data', 'name': 'Data', 'desc': 'Data line'},
    )
    annotations = (
        ('time', 'Time'),
    )
    annotation_rows = (
        ('time', 'Time', (0,)),
    )

    def __init__(self):
        self.samplerate = None
        self.oldpin = None
        self.last_samplenum = None

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
            # Ignore identical samples early on (for performance reasons).
            if self.oldpin == pin:
                continue

            if self.oldpin is None:
                self.oldpin = pin
                self.last_samplenum = self.samplenum
                continue

            if self.oldpin != pin:
                samples = self.samplenum - self.last_samplenum
                t = samples / self.samplerate

                # Report the timing normalized.
                self.put(self.last_samplenum, self.samplenum, self.out_ann,
                         [0, [normalize_time(t)]])

                # Store data for next round.
                self.last_samplenum = self.samplenum
                self.oldpin = pin
