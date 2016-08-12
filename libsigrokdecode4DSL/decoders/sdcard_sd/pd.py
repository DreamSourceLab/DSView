##
## This file is part of the sigrok project.
##
## Copyright (C) 2015 Uwe Hermann <uwe@hermann-uwe.de>
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

class Decoder(srd.Decoder):
    api_version = 2
    id = 'sdcard_sd'
    name = 'SD card (SD mode)'
    longname = 'Secure Digital card (SD mode)'
    desc = 'Secure Digital card (SD mode) low-level protocol.'
    license = 'gplv2+'
    inputs = ['logic']
    outputs = ['sdcard_sd']
    channels = (
        {'id': 'cmd',  'name': 'CMD',  'desc': 'Command'},
        {'id': 'clk',  'name': 'CLK',  'desc': 'Clock'},
    )
    optional_channels = (
        {'id': 'dat0', 'name': 'DAT0', 'desc': 'Data pin 0'},
        {'id': 'dat1', 'name': 'DAT1', 'desc': 'Data pin 1'},
        {'id': 'dat2', 'name': 'DAT2', 'desc': 'Data pin 2'},
        {'id': 'dat3', 'name': 'DAT3', 'desc': 'Data pin 3'},
    )
    annotations = \
        tuple(('cmd%d' % i, 'CMD%d' % i) for i in range(64)) + \
        tuple(('acmd%d' % i, 'ACMD%d' % i) for i in range(64)) + ( \
        ('bits', 'Bits'),
        ('field-start', 'Start bit'),
        ('field-transmission', 'Transmission bit'),
        ('field-cmd', 'Command'),
        ('field-arg', 'Argument'),
        ('field-crc', 'CRC'),
        ('field-end', 'End bit'),
        ('decoded-bits', 'Decoded bits'),
        ('decoded-fields', 'Decoded fields'),
    )
    annotation_rows = (
        ('raw-bits', 'Raw bits', (128,)),
        ('decoded-bits', 'Decoded bits', (135,)),
        ('decoded-fields', 'Decoded fields', (136,)),
        ('fields', 'Fields', tuple(range(129, 135))),
        ('cmd', 'Commands', tuple(range(128))),
    )

    def __init__(self, **kwargs):
        self.state = 'GET COMMAND TOKEN'
        self.token = []
        self.oldpins = None
        self.oldclk = 0
        self.is_acmd = False # Indicates CMD vs. ACMD
        self.cmd = None
        self.arg = None

    def start(self):
        self.out_ann = self.register(srd.OUTPUT_ANN)

    def putbit(self, b, data):
        self.put(self.token[b][0], self.token[b][1], self.out_ann, [135, data])

    def putt(self, data):
        self.put(self.token[0][0], self.token[47][1], self.out_ann, data)

    def putt2(self, data):
        self.put(self.token[47][0], self.token[0][1], self.out_ann, data)

    def putf(self, s, e, data):
        self.put(self.token[s][0], self.token[e][1], self.out_ann, data)

    def puta(self, s, e, data):
        self.put(self.token[47 - 8 - e][0], self.token[47 - 8 - s][1],
                 self.out_ann, data)

    def putc(self, cmd, desc):
        self.putt([cmd, ['%s: %s' % (self.cmd_str, desc), self.cmd_str,
                         self.cmd_str.split(' ')[0]]])

    def putr(self, cmd, desc):
        self.putt([cmd, ['Reply: %s' % desc]])

    def putr2(self, cmd, desc):
        self.putt2([cmd, ['Reply: %s' % desc]])

    def reset(self):
        self.cmd, self.arg = None, None
        self.token, self.state = [], 'GET COMMAND TOKEN'

    def cmd_name(self, cmd):
        c = acmd_names if self.is_acmd else cmd_names
        return c.get(cmd, 'Unknown')

    def get_token_bits(self, cmd, n):
        # Get a bit, return True if we already got 'n' bits, False otherwise.
        self.token.append([self.samplenum, self.samplenum, cmd])
        if len(self.token) > 0:
            self.token[len(self.token) - 2][1] = self.samplenum
        if len(self.token) < n:
            return False
        self.token[n - 1][1] += self.token[n - 1][0] - self.token[n - 2][0]
        return True

    def handle_common_token_fields(self):
        s = self.token

        # Annotations for each individual bit.
        for bit in range(len(self.token)):
            self.putf(bit, bit, [128, ['%d' % s[bit][2]]])

        # CMD[47:47]: Start bit (always 0)
        self.putf(0, 0, [129, ['Start bit', 'Start', 'S']])

        # CMD[46:46]: Transmission bit (1 == host)
        t = 'host' if s[1][2] == 1 else 'card'
        self.putf(1, 1, [130, ['Transmission: ' + t, 'T: ' + t, 'T']])

        # CMD[45:40]: Command index (BCD; valid: 0-63)
        self.cmd = int('0b' + ''.join([str(s[i][2]) for i in range(2, 8)]), 2)
        c = '%s (%d)' % (self.cmd_name(self.cmd), self.cmd)
        self.putf(2, 7, [131, ['Command: ' + c, 'Cmd: ' + c,
                               'CMD%d' % self.cmd, 'Cmd', 'C']])

        # CMD[39:08]: Argument
        self.putf(8, 39, [132, ['Argument', 'Arg', 'A']])

        # CMD[07:01]: CRC7
        self.crc = int('0b' + ''.join([str(s[i][2]) for i in range(40, 47)]), 2)
        self.putf(40, 46, [133, ['CRC: 0x%x' % self.crc, 'CRC', 'C']])

        # CMD[00:00]: End bit (always 1)
        self.putf(47, 47, [134, ['End bit', 'End', 'E']])

    def get_command_token(self, cmd):
        # Command tokens (48 bits) are sent serially (MSB-first) by the host
        # (over the CMD line), either to one SD card or to multiple ones.
        #
        # Format:
        #  - Bits[47:47]: Start bit (always 0)
        #  - Bits[46:46]: Transmission bit (1 == host)
        #  - Bits[45:40]: Command index (BCD; valid: 0-63)
        #  - Bits[39:08]: Argument
        #  - Bits[07:01]: CRC7
        #  - Bits[00:00]: End bit (always 1)

        if not self.get_token_bits(cmd, 48):
            return

        self.handle_common_token_fields()

        # Handle command.
        s = 'ACMD' if self.is_acmd else 'CMD'
        self.cmd_str = '%s%d (%s)' % (s, self.cmd, self.cmd_name(self.cmd))
        if self.cmd in (0, 2, 3, 4, 6, 7, 8, 9, 10, 13, 41, 51, 55):
            self.state = 'HANDLE CMD%d' % self.cmd
        else:
            self.state = 'HANDLE CMD999'
            self.putc(self.cmd, '%s%d' % (s, self.cmd))

    def handle_cmd0(self):
        # CMD0 (GO_IDLE_STATE) -> no response
        self.puta(0, 31, [136, ['Stuff bits', 'Stuff', 'SB', 'S']])
        self.putc(0, 'Reset all SD cards')
        self.token, self.state = [], 'GET COMMAND TOKEN'

    def handle_cmd2(self):
        # CMD2 (ALL_SEND_CID) -> R2
        self.puta(0, 31, [136, ['Stuff bits', 'Stuff', 'SB', 'S']])
        self.putc(2, 'Ask card for CID number')
        self.token, self.state = [], 'GET RESPONSE R2'

    def handle_cmd3(self):
        # CMD3 (SEND_RELATIVE_ADDR) -> R6
        self.puta(0, 31, [136, ['Stuff bits', 'Stuff', 'SB', 'S']])
        self.putc(3, 'Ask card for new relative card address (RCA)')
        self.token, self.state = [], 'GET RESPONSE R6'

    def handle_cmd6(self):
        # CMD6 (SWITCH_FUNC) -> R1
        self.putc(6, 'Switch/check card function')
        self.token, self.state = [], 'GET RESPONSE R1'

    def handle_cmd7(self):
        # CMD7 (SELECT/DESELECT_CARD) -> R1b
        self.putc(7, 'Select / deselect card')
        self.token, self.state = [], 'GET RESPONSE R6'

    def handle_cmd8(self):
        # CMD8 (SEND_IF_COND) -> R7
        self.puta(12, 31, [136, ['Reserved', 'Res', 'R']])
        self.puta(8, 11, [136, ['Supply voltage', 'Voltage', 'VHS', 'V']])
        self.puta(0, 7, [136, ['Check pattern', 'Check pat', 'Check', 'C']])
        self.putc(0, 'Send interface condition to card')
        self.token, self.state = [], 'GET RESPONSE R7'
        # TODO: Handle case when card doesn't reply with R7 (no reply at all).

    def handle_cmd9(self):
        # CMD9 (SEND_CSD) -> R2
        self.puta(16, 31, [136, ['RCA', 'R']])
        self.puta(0, 15, [136, ['Stuff bits', 'Stuff', 'SB', 'S']])
        self.putc(9, 'Send card-specific data (CSD)')
        self.token, self.state = [], 'GET RESPONSE R2'

    def handle_cmd10(self):
        # CMD10 (SEND_CID) -> R2
        self.puta(16, 31, [136, ['RCA', 'R']])
        self.puta(0, 15, [136, ['Stuff bits', 'Stuff', 'SB', 'S']])
        self.putc(9, 'Send card identification data (CID)')
        self.token, self.state = [], 'GET RESPONSE R2'

    def handle_cmd13(self):
        # CMD13 (SEND_STATUS) -> R1
        self.puta(16, 31, [136, ['RCA', 'R']])
        self.puta(0, 15, [136, ['Stuff bits', 'Stuff', 'SB', 'S']])
        self.putc(13, 'Send card status register')
        self.token, self.state = [], 'GET RESPONSE R1'

    def handle_cmd16(self):
        # CMD16 (SET_BLOCKLEN) -> R1
        self.blocklen = self.arg
        self.puta(0, 31, [136, ['Block length', 'Blocklen', 'BL', 'B']])
        self.putc(16, 'Set the block length to %d bytes' % self.blocklen)
        self.token, self.state = [], 'GET RESPONSE R1'

    def handle_cmd55(self):
        # CMD55 (APP_CMD) -> R1
        self.puta(16, 31, [136, ['RCA', 'R']])
        self.puta(0, 15, [136, ['Stuff bits', 'Stuff', 'SB', 'S']])
        self.putc(55, 'Next command is an application-specific command')
        self.is_acmd = True
        self.token, self.state = [], 'GET RESPONSE R1'

    def handle_acmd6(self):
        # ACMD6 (SET_BUS_WIDTH) -> R1
        self.putc(64 + 6, 'Read SD config register (SCR)')
        self.token, self.state = [], 'GET RESPONSE R1'

    def handle_acmd13(self):
        # ACMD13 (SD_STATUS) -> R1
        self.puta(0, 31, [136, ['Stuff bits', 'Stuff', 'SB', 'S']])
        self.putc(64 + 13, 'Set SD status')
        self.token, self.state = [], 'GET RESPONSE R1'

    def handle_acmd41(self):
        # ACMD41 (SD_SEND_OP_COND) -> R3
        self.puta(0, 23, [136, ['VDD voltage window', 'VDD volt', 'VDD', 'V']])
        self.puta(24, 24, [136, ['S18R']])
        self.puta(25, 27, [136, ['Reserved', 'Res', 'R']])
        self.puta(28, 28, [136, ['XPC']])
        self.puta(29, 29, [136, ['Reserved for eSD', 'Reserved', 'Res', 'R']])
        self.puta(30, 30, [136, ['Host capacity support info', 'Host capacity',
                                 'HCS', 'H']])
        self.puta(31, 31, [136, ['Reserved', 'Res', 'R']])
        self.putc(64 + 41, 'Send HCS info and activate the card init process')
        self.token, self.state = [], 'GET RESPONSE R3'

    def handle_acmd51(self):
        # ACMD51 (SEND_SCR) -> R1
        self.putc(64 + 51, 'Read SD config register (SCR)')
        self.token, self.state = [], 'GET RESPONSE R1'

    def handle_cmd999(self):
        self.token, self.state = [], 'GET RESPONSE R1'

    def handle_acmd999(self):
        self.token, self.state = [], 'GET RESPONSE R1'

    # Response tokens can have one of four formats (depends on content).
    # They can have a total length of 48 or 136 bits.
    # They're sent serially (MSB-first) by the card that the host
    # addressed previously, or (synchronously) by all connected cards.

    def handle_response_r1(self, cmd):
        # R1: Normal response command
        #  - Bits[47:47]: Start bit (always 0)
        #  - Bits[46:46]: Transmission bit (0 == card)
        #  - Bits[45:40]: Command index (BCD; valid: 0-63)
        #  - Bits[39:08]: Card status
        #  - Bits[07:01]: CRC7
        #  - Bits[00:00]: End bit (always 1)
        if not self.get_token_bits(cmd, 48):
            return
        self.handle_common_token_fields()
        self.putr(55, 'R1')
        self.puta(0, 31, [136, ['Card status', 'Status', 'S']])
        for i in range(32):
            self.putbit(8 + i, [card_status[31 - i]])
        self.token, self.state = [], 'GET COMMAND TOKEN'

    def handle_response_r1b(self, cmd):
        # R1b: Same as R1 with an optional busy signal (on the data line)
        if not self.get_token_bits(cmd, 48):
            return
        self.handle_common_token_fields()
        self.puta(0, 31, [136, ['Card status', 'Status', 'S']])
        self.putr(55, 'R1b')
        self.token, self.state = [], 'GET COMMAND TOKEN'

    def handle_response_r2(self, cmd):
        # R2: CID/CSD register
        #  - Bits[135:135]: Start bit (always 0)
        #  - Bits[134:134]: Transmission bit (0 == card)
        #  - Bits[133:128]: Reserved (always 0b111111)
        #  - Bits[127:001]: CID or CSD register including internal CRC7
        #  - Bits[000:000]: End bit (always 1)
        if not self.get_token_bits(cmd, 136):
            return
        # Annotations for each individual bit.
        for bit in range(len(self.token)):
            self.putf(bit, bit, [128, ['%d' % self.token[bit][2]]])
        self.putf(0, 0, [129, ['Start bit', 'Start', 'S']])
        t = 'host' if self.token[1][2] == 1 else 'card'
        self.putf(1, 1, [130, ['Transmission: ' + t, 'T: ' + t, 'T']])
        self.putf(2, 7, [131, ['Reserved', 'Res', 'R']])
        self.putf(8, 134, [132, ['Argument', 'Arg', 'A']])
        self.putf(135, 135, [134, ['End bit', 'End', 'E']])
        self.putf(8, 134, [136, ['CID/CSD register', 'CID/CSD', 'C']])
        self.putf(0, 135, [55, ['R2']])
        self.token, self.state = [], 'GET COMMAND TOKEN'

    def handle_response_r3(self, cmd):
        # R3: OCR register
        #  - Bits[47:47]: Start bit (always 0)
        #  - Bits[46:46]: Transmission bit (0 == card)
        #  - Bits[45:40]: Reserved (always 0b111111)
        #  - Bits[39:08]: OCR register
        #  - Bits[07:01]: Reserved (always 0b111111)
        #  - Bits[00:00]: End bit (always 1)
        if not self.get_token_bits(cmd, 48):
            return
        self.putr(55, 'R3')
        # Annotations for each individual bit.
        for bit in range(len(self.token)):
            self.putf(bit, bit, [128, ['%d' % self.token[bit][2]]])
        self.putf(0, 0, [129, ['Start bit', 'Start', 'S']])
        t = 'host' if self.token[1][2] == 1 else 'card'
        self.putf(1, 1, [130, ['Transmission: ' + t, 'T: ' + t, 'T']])
        self.putf(2, 7, [131, ['Reserved', 'Res', 'R']])
        self.putf(8, 39, [132, ['Argument', 'Arg', 'A']])
        self.putf(40, 46, [133, ['Reserved', 'Res', 'R']])
        self.putf(47, 47, [134, ['End bit', 'End', 'E']])
        self.puta(0, 31, [136, ['OCR register', 'OCR reg', 'OCR', 'O']])
        self.token, self.state = [], 'GET COMMAND TOKEN'

    def handle_response_r6(self, cmd):
        # R6: Published RCA response
        #  - Bits[47:47]: Start bit (always 0)
        #  - Bits[46:46]: Transmission bit (0 == card)
        #  - Bits[45:40]: Command index (always 0b000011)
        #  - Bits[39:24]: Argument[31:16]: New published RCA of the card
        #  - Bits[23:08]: Argument[15:0]: Card status bits
        #  - Bits[07:01]: CRC7
        #  - Bits[00:00]: End bit (always 1)
        if not self.get_token_bits(cmd, 48):
            return
        self.handle_common_token_fields()
        self.puta(0, 15, [136, ['Card status bits', 'Status', 'S']])
        self.puta(16, 31, [136, ['Relative card address', 'RCA', 'R']])
        self.putr(55, 'R6')
        self.token, self.state = [], 'GET COMMAND TOKEN'

    def handle_response_r7(self, cmd):
        # R7: Card interface condition
        #  - Bits[47:47]: Start bit (always 0)
        #  - Bits[46:46]: Transmission bit (0 == card)
        #  - Bits[45:40]: Command index (always 0b001000)
        #  - Bits[39:20]: Reserved bits (all-zero)
        #  - Bits[19:16]: Voltage accepted
        #  - Bits[15:08]: Echo-back of check pattern
        #  - Bits[07:01]: CRC7
        #  - Bits[00:00]: End bit (always 1)
        if not self.get_token_bits(cmd, 48):
            return
        self.handle_common_token_fields()

        self.putr(55, 'R7')

        # Arg[31:12]: Reserved bits (all-zero)
        self.puta(12, 31, [136, ['Reserved', 'Res', 'R']])

        # Arg[11:08]: Voltage accepted
        v = ''.join(str(i[2]) for i in self.token[28:32])
        av = accepted_voltages.get(int('0b' + v, 2), 'Unknown')
        self.puta(8, 11, [136, ['Voltage accepted: ' + av, 'Voltage', 'Volt', 'V']])

        # Arg[07:00]: Echo-back of check pattern
        self.puta(0, 7, [136, ['Echo-back of check pattern', 'Echo', 'E']])

        self.token, self.state = [], 'GET COMMAND TOKEN'

    def decode(self, ss, es, data):
        for (self.samplenum, pins) in data:
            data.itercnt += 1
            # Ignore identical samples early on (for performance reasons).
            if self.oldpins == pins:
                continue
            self.oldpins, (cmd, clk, dat0, dat1, dat2, dat3) = pins, pins

            # Wait for a rising CLK edge.
            if not (self.oldclk == 0 and clk == 1):
                self.oldclk = clk
                continue
            self.oldclk = clk

            # State machine.
            if self.state == 'GET COMMAND TOKEN':
                if len(self.token) == 0:
                    # Wait for start bit (CMD = 0).
                    if cmd != 0:
                        continue
                self.get_command_token(cmd)
            elif self.state.startswith('HANDLE CMD'):
                # Call the respective handler method for the command.
                a, cmdstr = 'a' if self.is_acmd else '', self.state[10:].lower()
                handle_cmd = getattr(self, 'handle_%scmd%s' % (a, cmdstr))
                handle_cmd()
                # Leave ACMD mode again after the first command after CMD55.
                if self.is_acmd and cmdstr not in ('55', '63'):
                    self.is_acmd = False
            elif self.state.startswith('GET RESPONSE'):
                if len(self.token) == 0:
                    # Wait for start bit (CMD = 0).
                    if cmd != 0:
                        continue
                # Call the respective handler method for the response.
                s = 'handle_response_%s' % self.state[13:].lower()
                handle_response = getattr(self, s)
                handle_response(cmd)
