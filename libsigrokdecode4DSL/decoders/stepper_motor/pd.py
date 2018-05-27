##
## This file is part of the libsigrokdecode project.
##
## Copyright (C) 2015 Petteri Aimonen <jpa@sigrok.mail.kapsi.fi>
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
    id = 'stepper_motor'
    name = 'Stepper motor'
    longname = 'Stepper motor position / speed'
    desc = 'Absolute position and movement speed from step/dir.'
    license = 'gplv2+'
    inputs = ['logic']
    outputs = ['stepper_motor']
    channels = (
        {'id': 'step', 'name': 'Step', 'desc': 'Step pulse'},
        {'id': 'dir', 'name': 'Direction', 'desc': 'Direction select'},
    )
    options = (
        {'id': 'unit', 'desc': 'Unit', 'default': 'steps',
         'values': ('steps', 'mm')},
        {'id': 'steps_per_mm', 'desc': 'Steps per mm', 'default': 100.0},
    )
    annotations = (
        ('speed', 'Speed'),
        ('position', 'Position')
    )
    annotation_rows = (
        ('speed', 'Speed', (0,)),
        ('position', 'Position', (1,)),
    )

    def __init__(self):
        self.oldstep = None
        self.ss_prev_step = None
        self.pos = 0
        self.prev_speed = None
        self.prev_pos = None

    def start(self):
        self.out_ann = self.register(srd.OUTPUT_ANN)

        if self.options['unit'] == 'steps':
            self.scale = 1
            self.format = '%0.0f'
            self.unit = 'steps'
        else:
            self.scale = self.options['steps_per_mm']
            self.format = '%0.2f'
            self.unit = 'mm'

    def step(self, ss, direction):
        if self.ss_prev_step is not None:
            delta = ss - self.ss_prev_step
            speed = self.samplerate / delta / self.scale
            speed_txt = self.format % speed
            pos_txt = self.format % (self.pos / self.scale)
            self.put(self.ss_prev_step, ss, self.out_ann,
                [0, [speed_txt + ' ' + self.unit + '/s', speed_txt]])
            self.put(self.ss_prev_step, ss, self.out_ann,
                [1, [pos_txt + ' ' + self.unit, pos_txt]])

        self.pos += (1 if direction else -1)
        self.ss_prev_step = ss

    def metadata(self, key, value):
        if key == srd.SRD_CONF_SAMPLERATE:
            self.samplerate = value

    def decode(self, ss, es, data):
        if not self.samplerate:
            raise SamplerateError('Cannot decode without samplerate.')

        for (self.samplenum, (step, direction)) in data:
            data.itercnt += 1
            if step == 1 and self.oldstep == 0:
                self.step(self.samplenum, direction)
            self.oldstep = step
