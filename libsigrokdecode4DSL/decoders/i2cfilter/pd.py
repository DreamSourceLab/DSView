##
## This file is part of the libsigrokdecode project.
##
## Copyright (C) 2012 Bert Vermeulen <bert@biot.com>
## Copyright (C) 2012 Uwe Hermann <uwe@hermann-uwe.de>
##
## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 3 of the License, or
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

# TODO: Support for filtering out multiple slave/direction pairs?

import sigrokdecode as srd

class Decoder(srd.Decoder):
    api_version = 2
    id = 'i2cfilter'
    name = 'I²C filter'
    longname = 'I²C filter'
    desc = 'Filter out addresses/directions in an I²C stream.'
    license = 'gplv3+'
    inputs = ['i2c']
    outputs = ['i2c']
    options = (
        {'id': 'address', 'desc': 'Address to filter out of the I²C stream',
            'default': 0},
        {'id': 'direction', 'desc': 'Direction to filter', 'default': 'both',
            'values': ('read', 'write', 'both')}
    )

    def __init__(self):
        self.curslave = -1
        self.curdirection = None
        self.packets = [] # Local cache of I²C packets

    def start(self):
        self.out_python = self.register(srd.OUTPUT_PYTHON, proto_id='i2c')
        if self.options['address'] not in range(0, 127 + 1):
            raise Exception('Invalid slave (must be 0..127).')

    # Grab I²C packets into a local cache, until an I²C STOP condition
    # packet comes along. At some point before that STOP condition, there
    # will have been an ADDRESS READ or ADDRESS WRITE which contains the
    # I²C address of the slave that the master wants to talk to.
    # If that slave shall be filtered, output the cache (all packets from
    # START to STOP) as proto 'i2c', otherwise drop it.
    def decode(self, ss, es, data):

        cmd, databyte = data

        # Add the I²C packet to our local cache.
        self.packets.append([ss, es, data])

        if cmd in ('ADDRESS READ', 'ADDRESS WRITE'):
            self.curslave = databyte
            self.curdirection = cmd[8:].lower()
        elif cmd in ('STOP', 'START REPEAT'):
            # If this chunk was not for the correct slave, drop it.
            if self.options['address'] == 0:
                pass
            elif self.curslave != self.options['address']:
                self.packets = []
                return

            # If this chunk was not in the right direction, drop it.
            if self.options['direction'] == 'both':
                pass
            elif self.options['direction'] != self.curdirection:
                self.packets = []
                return

            # TODO: START->STOP chunks with both read and write (Repeat START)
            # Otherwise, send out the whole chunk of I²C packets.
            for p in self.packets:
                self.put(p[0], p[1], self.out_python, p[2])

            self.packets = []
        else:
            pass # Do nothing, only add the I²C packet to our cache.
