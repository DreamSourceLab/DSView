##
## This file is part of the libsigrokdecode project.
##
## Copyright (C) 2020 Richard Li <richard.li@ces.hk>
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

class ChannelError(Exception):
    pass

regs = {
#   addr: ('name',        size)
    0x00: ('MODE',           1),
    0x01: ('MODE_CTRL',      1),
    0x02: ('CALC',           1),
    0x03: ('FIFO_I',         1),
    0x04: ('FIFO_II',        1),
    0x05: ('FIFO_DATA',      1),
    0x06: ('ID_DATA',        1),
    0x07: ('RC_OSC_I',       1),
    0x08: ('RC_OSC_II',      1),
    0x09: ('RC_OSC_III',     1),
    0x0a: ('CKO_PIN',        1),
    0x0b: ('GPIO1_PIN_I',    1),
    0x0c: ('GPIO2_PIN_II',   1),
    0x0d: ('CLOCK',          1),
    0x0e: ('DATA_RATE',      1),
    0x0f: ('PLL_I',          1),
    0x10: ('PLL_II',         1),
    0x11: ('PLL_III',        1),
    0x12: ('PLL_IV',         1),
    0x13: ('PLL_V',          1),
    0x14: ('TX_I',           1),
    0x15: ('TX_II',          1),
    0x16: ('DELAY_I',        1),
    0x17: ('DELAY_II',       1),
    0x18: ('RX',             1),
    0x19: ('RX_GAIN_I',      1),
    0x1a: ('RX_GAIN_II',     1),
    0x1b: ('RX_GAIN_III',    1),
    0x1c: ('RX_GAIN_IV',     1),
    0x1d: ('RSSI_THRES',     1),
    0x1e: ('ADC',            1),
    0x1f: ('CODE_I',         1),
    0x20: ('CODE_II',        1),
    0x21: ('CODE_III',       1),
    0x22: ('IF_CAL_I',       1),
    0x23: ('IF_CAL_II',      1),
    0x24: ('VCO_CURR_CAL',   1),
    0x25: ('VCO_SB_CALC_I',  1),
    0x26: ('VCO_SB_CALC_II', 1),
    0x27: ('BATT_DETECT',    1),
    0x28: ('TX_TEST',        1),
    0x29: ('RX_DEM_TEST_I',  1),
    0x2a: ('RX_DEM_TEST_II', 1),
    0x2b: ('CPC',            1),
    0x2c: ('CRYSTAL_TEST',   1),
    0x2d: ('PLL_TEST',       1),
    0x2e: ('VCO_TEST_I',     1),
    0x2f: ('VCO_TEST_II',    1),
    0x30: ('IFAT',           1),
    0x31: ('RSCALE',         1),
    0x32: ('FILTER_TEST',    1),
    0x33: ('UNKNOWN',        1),
}

class Decoder(srd.Decoder):
    api_version = 3
    id = 'a7105'
    name = 'A7105'
    longname = 'AMICCOM A7105'
    desc = '2.4GHz FSK/GFSK Transceiver with 2K ~ 500Kbps data rate.'
    license = 'gplv2+'
    inputs = ['spi']
    outputs = []
    tags = ['IC', 'Wireless/RF']
    options = (
        {'id': 'hex_display', 'desc': 'Display payload in Hex', 'default': 'yes',
            'values': ('yes', 'no')},
    )
    annotations = (
        # Sent from the host to the chip.
        ('cmd', 'Commands sent to the device'),
        ('tx-data', 'Payload sent to the device'),

        # Returned by the chip.
        ('rx-data', 'Payload read from the device'),

        ('warning', 'Warnings'),
    )
    ann_cmd = 0
    ann_tx = 1
    ann_rx = 2
    ann_warn = 3
    annotation_rows = (
        ('commands', 'Commands', (ann_cmd, ann_tx, ann_rx)),
        ('warnings', 'Warnings', (ann_warn,)),
    )

    def __init__(self):
        self.reset()

    def reset(self):
        self.next()
        self.requirements_met = True
        self.cs_was_released = False

    def start(self):
        self.out_ann = self.register(srd.OUTPUT_ANN)

    def warn(self, pos, msg):
        '''Put a warning message 'msg' at 'pos'.'''
        self.put(pos[0], pos[1], self.out_ann, [self.ann_warn, [msg]])

    def putp(self, pos, ann, msg):
        '''Put an annotation message 'msg' at 'pos'.'''
        self.put(pos[0], pos[1], self.out_ann, [ann, [msg]])

    def next(self):
        '''Resets the decoder after a complete command was decoded.'''
        # 'True' for the first byte after CS went low.
        self.first = True

        # The current command, and the minimum and maximum number
        # of data bytes to follow.
        self.cmd = None
        self.min = 0
        self.max = 0

        # Used to collect the bytes after the command byte
        # (and the start/end sample number).
        self.mb = []
        self.mb_s = -1
        self.mb_e = -1

    def mosi_bytes(self):
        '''Returns the collected MOSI bytes of a multi byte command.'''
        return [b[0] for b in self.mb]

    def miso_bytes(self):
        '''Returns the collected MISO bytes of a multi byte command.'''
        return [b[1] for b in self.mb]

    def decode_command(self, pos, b):
        '''Decodes the command byte 'b' at position 'pos' and prepares
        the decoding of the following data bytes.'''
        c = self.parse_command(b)
        if c is None:
            self.warn(pos, 'unknown command')
            return

        self.cmd, self.dat, self.min, self.max = c

        if self.cmd in ('W_REGISTER', 'R_REGISTER'):
            # Don't output anything now, the command is merged with
            # the data bytes following it.
            self.mb_s = pos[0]
        else:
            self.putp(pos, self.ann_cmd, self.format_command())

    def format_command(self):
        '''Returns the label for the current command.'''
        return 'Cmd {}'.format(self.cmd)

    def parse_command(self, b):
        '''Parses the command byte.

        Returns a tuple consisting of:
        - the name of the command
        - additional data needed to dissect the following bytes
        - minimum number of following bytes
        - maximum number of following bytes
        '''
        
        if b == 0x05:
            return ('W_TX_FIFO', None, 1, 32)
        elif b == 0x45:
            return ('R_RX_FIFO', None, 1, 32)            
        if b == 0x06:
            return ('W_ID', None, 1, 4)
        elif b == 0x46:
            return ('R_ID', None, 1, 4)
        elif (b & 0b10000000) == 0:
            if (b & 0b01000000) == 0:
                c = 'W_REGISTER'
            else:
                c = 'R_REGISTER'
            d = b & 0b00111111
            return (c, d, 1, 1)
		
        else:
            cmd = b & 0b11110000        
            if cmd == 0b10000000:
                return ('SLEEP_MODE', None, 0, 0)
            if cmd == 0b10010000:
                return ('IDLE_MODE', None, 0, 0)
            if cmd == 0b10100000:
                return ('STANDBY_MODE', None, 0, 0)
            if cmd == 0b10110000:
                return ('PLL_MODE', None, 0, 0)
            if cmd == 0b11000000:
                return ('RX_MODE', None, 0, 0)
            if cmd == 0b11010000:
                return ('TX_MODE', None, 0, 0)
            if cmd == 0b11100000:
                return ('FIFO_WRITE_PTR_RESET', None, 0, 0)
            if cmd == 0b11110000:
                return ('FIFO_READ_PTR_RESET', None, 0, 0)

    def decode_register(self, pos, ann, regid, data):
        '''Decodes a register.

        pos   -- start and end sample numbers of the register
        ann   -- is the annotation number that is used to output the register.
        regid -- may be either an integer used as a key for the 'regs'
                 dictionary, or a string directly containing a register name.'
        data  -- is the register content.
        '''

        if type(regid) == int:
            # Get the name of the register.
            if regid not in regs:
                self.warn(pos, 'unknown register')
                return
            name = regs[regid][0]
        else:
            name = regid

        # Multi byte register come LSByte first.
        data = reversed(data)

        label = '{}: {}'.format(self.format_command(), name)

        self.decode_mb_data(pos, ann, data, label, True)

    def decode_mb_data(self, pos, ann, data, label, always_hex):
        '''Decodes the data bytes 'data' of a multibyte command at position
        'pos'. The decoded data is prefixed with 'label'. If 'always_hex' is
        True, all bytes are decoded as hex codes, otherwise only non
        printable characters are escaped.'''

        if always_hex:
            def escape(b):
                return '{:02X}'.format(b)
        else:
            def escape(b):
                c = chr(b)
                if not str.isprintable(c):
                    return '\\x{:02X}'.format(b)
                return c

        data = ''.join([escape(b) for b in data])
        text = '{} = "{}"'.format(label, data.strip())
        self.putp(pos, ann, text)

    def finish_command(self, pos):
        '''Decodes the remaining data bytes at position 'pos'.'''

        always_hex = self.options['hex_display'] == 'yes'
        if self.cmd == 'R_REGISTER':
            self.decode_register(pos, self.ann_cmd,
                                 self.dat, self.miso_bytes())
        elif self.cmd == 'W_REGISTER':
            self.decode_register(pos, self.ann_cmd,
                                 self.dat, self.mosi_bytes())			
        elif self.cmd == 'R_RX_FIFO':
            self.decode_mb_data(pos, self.ann_rx,
                                self.miso_bytes(), 'RX FIFO', always_hex)
        elif self.cmd == 'W_TX_FIFO':
            self.decode_mb_data(pos, self.ann_tx,
                                self.mosi_bytes(), 'TX FIFO', always_hex)
        elif self.cmd == 'R_ID':
            self.decode_mb_data(pos, self.ann_rx,
                                self.miso_bytes(), 'R ID', always_hex)
        elif self.cmd == 'W_ID':
            self.decode_mb_data(pos, self.ann_tx,
                                self.mosi_bytes(), 'W ID', always_hex)

    def decode(self, ss, es, data):
        if not self.requirements_met:
            return

        ptype, data1, data2 = data

        if ptype == 'TRANSFER':
            if self.cmd:
                # Check if we got the minimum number of data bytes
                # after the command byte.
                if len(self.mb) < self.min:
                    self.warn((ss, ss), 'missing data bytes')
                elif self.mb:
                    self.finish_command((self.mb_s, self.mb_e))

            self.next()
            self.cs_was_released = True
        elif ptype == 'CS-CHANGE':
            if data1 is None:
                if data2 is None:
                    self.requirements_met = False
                    raise ChannelError('CS# pin required.')
                elif data2 == 1:
                    self.cs_was_released = True

            if data1 == 0 and data2 == 1:
                # Rising edge, the complete command is transmitted, process
                # the bytes that were send after the command byte.
                if self.cmd:
                    # Check if we got the minimum number of data bytes
                    # after the command byte.
                    if len(self.mb) < self.min:
                        self.warn((ss, ss), 'missing data bytes')
                    elif self.mb:
                        self.finish_command((self.mb_s, self.mb_e))

                self.next()
                self.cs_was_released = True
        elif ptype == 'DATA' and self.cs_was_released:
            mosi, miso = data1, data2
            pos = (ss, es)

            if miso is None and mosi is None:
                self.requirements_met = False
                raise ChannelError('Either MISO or MOSI pins required (3 wires SPI).')

            if miso is None:
                miso = mosi
            if mosi is None:
                mosi = miso

            if self.first:
                self.first = False
                # First byte is always the command.
                self.decode_command(pos, mosi)
            else:
                if not self.cmd or len(self.mb) >= self.max:
                    self.warn(pos, 'excess byte')
                else:
                    # Collect the bytes after the command byte.
                    if self.mb_s == -1:
                        self.mb_s = ss
                    self.mb_e = es
                    self.mb.append((mosi, miso))
