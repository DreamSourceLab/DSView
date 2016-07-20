##
## This file is part of the libsigrokdecode project.
##
## Copyright (C) 2013 Uwe Hermann <uwe@hermann-uwe.de>
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

RX = 0
TX = 1

class Decoder(srd.Decoder):
    api_version = 2
    id = 'midi'
    name = 'MIDI'
    longname = 'Musical Instrument Digital Interface'
    desc = 'Musical Instrument Digital Interface (MIDI) protocol.'
    license = 'gplv2+'
    inputs = ['uart']
    outputs = ['midi']
    annotations = (
        ('text-verbose', 'Human-readable text (verbose)'),
    )

    def __init__(self):
        self.cmd = []
        self.state = 'IDLE'
        self.ss = None
        self.es = None
        self.ss_block = None
        self.es_block = None

    def start(self):
        self.out_ann = self.register(srd.OUTPUT_ANN)

    def putx(self, data):
        self.put(self.ss_block, self.es_block, self.out_ann, data)

    def handle_channel_msg_0x80(self):
        # Note off: 8n kk vv
        # n = channel, kk = note, vv = velocity
        c = self.cmd
        if len(c) < 3:
            return
        self.es_block = self.es
        msg, chan, note, velocity = c[0] & 0xf0, (c[0] & 0x0f) + 1, c[1], c[2]
        self.putx([0, ['Channel %d: %s (note = %d, velocity = %d)' % \
                  (chan, status_bytes[msg], note, velocity)]])
        self.cmd, self.state = [], 'IDLE'

    def handle_channel_msg_0x90(self):
        # Note on: 9n kk vv
        # n = channel, kk = note, vv = velocity
        # If velocity == 0 that actually means 'note off', though.
        c = self.cmd
        if len(c) < 3:
            return
        self.es_block = self.es
        msg, chan, note, velocity = c[0] & 0xf0, (c[0] & 0x0f) + 1, c[1], c[2]
        s = 'note off' if (velocity == 0) else status_bytes[msg]
        self.putx([0, ['Channel %d: %s (note = %d, velocity = %d)' % \
                  (chan, s, note, velocity)]])
        self.cmd, self.state = [], 'IDLE'

    def handle_channel_msg_0xa0(self):
        # Polyphonic key pressure / aftertouch: An kk vv
        # n = channel, kk = polyphonic key pressure, vv = pressure value
        pass # TODO

    def handle_controller_0x44(self):
        # Legato footswitch: Bn 44 vv
        # n = channel, vv = value (<= 0x3f: normal, > 0x3f: legato)
        chan, vv = (self.cmd[0] & 0x0f) + 1, self.cmd[2]
        t = 'normal' if vv <= 0x3f else 'legato'
        self.putx([0, ['Channel %d: control function \'%s\' = %s' % \
                  (chan, control_functions[0x44], t)]])

    def handle_controller_0x54(self):
        # Portamento control (PTC): Bn 54 kk
        # n = channel, kk = source note for pitch reference
        chan, kk = (self.cmd[0] & 0x0f) + 1, self.cmd[2]
        self.putx([0, ['Channel %d: control function \'%s\' (source note ' \
                  '= %d)' % (chan, control_functions[0x54], kk)]])

    def handle_controller_generic(self):
        c = self.cmd
        chan, fn, param = (c[0] & 0x0f) + 1, c[1], c[2]
        ctrl_fn = control_functions.get(fn, 'undefined')
        self.putx([0, ['Channel %d: control change to function \'%s\' ' \
                  '(param = 0x%02x)' % (chan, ctrl_fn, param)]])

    def handle_channel_msg_0xb0(self):
        # Control change (or channel mode messages): Bn cc vv
        # n = channel, cc = control number (0 - 119), vv = control value
        c = self.cmd
        if (len(c) >= 2) and (c[1] in range(0x78, 0x7f + 1)):
            # This is not a control change, but rather a channel mode message.
            # TODO: Handle channel mode messages.
            return
        if len(c) < 3:
            return
        self.es_block = self.es
        handle_ctrl = getattr(self, 'handle_controller_0x%02x' % c[1],
                              self.handle_controller_generic)
        handle_ctrl()
        self.cmd, self.state = [], 'IDLE'

    def handle_channel_msg_0xc0(self):
        # Program change: Cn pp
        # n = channel, pp = program number (0 - 127)
        pass # TODO

    def handle_channel_msg_0xd0(self):
        # Channel pressure / aftertouch: Dn vv
        # n = channel, vv = pressure value
        pass # TODO

    def handle_channel_msg_0xe0(self):
        # Pitch bend change: En ll mm
        # n = channel, ll = pitch bend change LSB, mm = pitch bend change MSB
        pass # TODO

    def handle_channel_msg_generic(self):
        msg_type = self.cmd[0] & 0xf0
        self.putx([0, ['Unknown channel message type: 0x%02x' % msg_type]])
        # TODO: Handle properly.

    def handle_channel_msg(self, newbyte):
        self.cmd.append(newbyte)
        msg_type = self.cmd[0] & 0xf0
        handle_msg = getattr(self, 'handle_channel_msg_0x%02x' % msg_type,
                             self.handle_channel_msg_generic)
        handle_msg()

    def handle_sysex_msg(self, newbyte):
        # SysEx message: 1 status byte, x data bytes, EOX byte
        self.cmd.append(newbyte)
        if newbyte != 0xf7: # EOX
            return
        self.es_block = self.es
        # TODO: Get message ID, vendor ID, message contents, etc.
        self.putx([0, ['SysEx message']])
        self.cmd, self.state = [], 'IDLE'

    def handle_syscommon_msg(self, newbyte):
        pass # TODO

    def handle_sysrealtime_msg(self, newbyte):
        # System realtime message: 0b11111ttt (t = message type)
        self.es_block = self.es
        self.putx([0, ['System realtime message: %s' % status_bytes[newbyte]]])
        self.cmd, self.state = [], 'IDLE'

    def decode(self, ss, es, data):
        ptype, rxtx, pdata = data

        # For now, ignore all UART packets except the actual data packets.
        if ptype != 'DATA':
            return

        self.ss, self.es = ss, es

        # We're only interested in the byte value (not individual bits).
        pdata = pdata[0]

        # Short MIDI overview:
        #  - Status bytes are 0x80-0xff, data bytes are 0x00-0x7f.
        #  - Most messages: 1 status byte, 1-2 data bytes.
        #  - Real-time system messages: always 1 byte.
        #  - SysEx messages: 1 status byte, n data bytes, EOX byte.

        # State machine.
        if self.state == 'IDLE':
            # Wait until we see a status byte (bit 7 must be set).
            if pdata < 0x80:
                return # TODO: How to handle? Ignore?
            # This is a status byte, remember the start sample.
            self.ss_block = ss
            if pdata in range(0x80, 0xef + 1):
                self.state = 'HANDLE CHANNEL MSG'
            elif pdata == 0xf0:
                self.state = 'HANDLE SYSEX MSG'
            elif pdata in range(0xf1, 0xf7 + 1):
                self.state = 'HANDLE SYSCOMMON MSG'
            elif pdata in range(0xf8, 0xff + 1):
                self.state = 'HANDLE SYSREALTIME MSG'

        # Yes, this is intentionally _not_ an 'elif' here.
        if self.state == 'HANDLE CHANNEL MSG':
            self.handle_channel_msg(pdata)
        elif self.state == 'HANDLE SYSEX MSG':
            self.handle_sysex_msg(pdata)
        elif self.state == 'HANDLE SYSCOMMON MSG':
            self.handle_syscommon_msg(pdata)
        elif self.state == 'HANDLE SYSREALTIME MSG':
            self.handle_sysrealtime_msg(pdata)
