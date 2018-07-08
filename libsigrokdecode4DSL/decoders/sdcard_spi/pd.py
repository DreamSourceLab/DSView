##
## This file is part of the libsigrokdecode project.
##
## Copyright (C) 2012-2014 Uwe Hermann <uwe@hermann-uwe.de>
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

# Normal commands (CMD)
cmd_names = {
    0:  'GO_IDLE_STATE',
    1:  'SEND_OP_COND',
    6:  'SWITCH_FUNC',
    8:  'SEND_IF_COND',
    9:  'SEND_CSD',
    10: 'SEND_CID',
    12: 'STOP_TRANSMISSION',
    13: 'SEND_STATUS',
    16: 'SET_BLOCKLEN',
    17: 'READ_SINGLE_BLOCK',
    18: 'READ_MULTIPLE_BLOCK',
    24: 'WRITE_BLOCK',
    25: 'WRITE_MULTIPLE_BLOCK',
    27: 'PROGRAM_CSD',
    28: 'SET_WRITE_PROT',
    29: 'CLR_WRITE_PROT',
    30: 'SEND_WRITE_PROT',
    32: 'ERASE_WR_BLK_START_ADDR',
    33: 'ERASE_WR_BLK_END_ADDR',
    38: 'ERASE',
    42: 'LOCK_UNLOCK',
    55: 'APP_CMD',
    56: 'GEN_CMD',
    58: 'READ_OCR',
    59: 'CRC_ON_OFF',
    # CMD60-63: Reserved for manufacturer
}

# Application-specific commands (ACMD)
acmd_names = {
    13: 'SD_STATUS',
    18: 'Reserved for SD security applications',
    22: 'SEND_NUM_WR_BLOCKS',
    23: 'SET_WR_BLK_ERASE_COUNT',
    25: 'Reserved for SD security applications',
    26: 'Reserved for SD security applications',
    38: 'Reserved for SD security applications',
    41: 'SD_SEND_OP_COND',
    42: 'SET_CLR_CARD_DETECT',
    43: 'Reserved for SD security applications',
    44: 'Reserved for SD security applications',
    45: 'Reserved for SD security applications',
    46: 'Reserved for SD security applications',
    47: 'Reserved for SD security applications',
    48: 'Reserved for SD security applications',
    49: 'Reserved for SD security applications',
    51: 'SEND_SCR',
}

class Decoder(srd.Decoder):
    api_version = 2
    id = 'sdcard_spi'
    name = 'SD card (SPI mode)'
    longname = 'Secure Digital card (SPI mode)'
    desc = 'Secure Digital card (SPI mode) low-level protocol.'
    license = 'gplv2+'
    inputs = ['spi']
    outputs = ['sdcard_spi']
    annotations = \
        tuple(('cmd%d' % i, 'CMD%d' % i) for i in range(64)) + \
        tuple(('acmd%d' % i, 'ACMD%d' % i) for i in range(64)) + ( \
        ('r1', 'R1 reply'),
        ('r1b', 'R1B reply'),
        ('r2', 'R2 reply'),
        ('r3', 'R3 reply'),
        ('r7', 'R7 reply'),
        ('bits', 'Bits'),
        ('bit-warnings', 'Bit warnings'),
    )
    annotation_rows = (
        ('bits', 'Bits', (134, 135)),
        ('cmd-reply', 'Commands/replies', tuple(range(134))),
    )

    def __init__(self, **kwargs):
        self.state = 'IDLE'
        self.ss, self.es = 0, 0
        self.ss_bit, self.es_bit = 0, 0
        self.ss_cmd, self.es_cmd = 0, 0
        self.cmd_token = []
        self.cmd_token_bits = []
        self.is_acmd = False # Indicates CMD vs. ACMD
        self.blocklen = 0
        self.read_buf = []
        self.cmd_str = ''

    def start(self):
        self.out_ann = self.register(srd.OUTPUT_ANN)

    def putx(self, data):
        self.put(self.ss_cmd, self.es_cmd, self.out_ann, data)

    def putc(self, cmd, desc):
        self.putx([cmd, ['%s: %s' % (self.cmd_str, desc)]])

    def putb(self, data):
        self.put(self.ss_bit, self.es_bit, self.out_ann, data)

    def cmd_name(self, cmd):
        c = acmd_names if self.is_acmd else cmd_names
        return c.get(cmd, 'Unknown')

    def handle_command_token(self, mosi, miso):
        # Command tokens (6 bytes) are sent (MSB-first) by the host.
        #
        # Format:
        #  - CMD[47:47]: Start bit (always 0)
        #  - CMD[46:46]: Transmitter bit (1 == host)
        #  - CMD[45:40]: Command index (BCD; valid: 0-63)
        #  - CMD[39:08]: Argument
        #  - CMD[07:01]: CRC7
        #  - CMD[00:00]: End bit (always 1)

        if len(self.cmd_token) == 0:
            self.ss_cmd = self.ss

        self.cmd_token.append(mosi)
        self.cmd_token_bits.append(self.mosi_bits)

        # All command tokens are 6 bytes long.
        if len(self.cmd_token) < 6:
            return

        self.es_cmd = self.es

        t = self.cmd_token

        # CMD or ACMD?
        s = 'ACMD' if self.is_acmd else 'CMD'

        def tb(byte, bit):
            return self.cmd_token_bits[5 - byte][bit]

        # Bits[47:47]: Start bit (always 0)
        bit, self.ss_bit, self.es_bit = tb(5, 7)[0], tb(5, 7)[1], tb(5, 7)[2]
        if bit == 0:
            self.putb([134, ['Start bit: %d' % bit]])
        else:
            self.putb([135, ['Start bit: %s (Warning: Must be 0!)' % bit]])

        # Bits[46:46]: Transmitter bit (1 == host)
        bit, self.ss_bit, self.es_bit = tb(5, 6)[0], tb(5, 6)[1], tb(5, 6)[2]
        if bit == 1:
            self.putb([134, ['Transmitter bit: %d' % bit]])
        else:
            self.putb([135, ['Transmitter bit: %d (Warning: Must be 1!)' % bit]])

        # Bits[45:40]: Command index (BCD; valid: 0-63)
        cmd = self.cmd_index = t[0] & 0x3f
        self.ss_bit, self.es_bit = tb(5, 5)[1], tb(5, 0)[2]
        self.putb([134, ['Command: %s%d (%s)' % (s, cmd, self.cmd_name(cmd))]])

        # Bits[39:8]: Argument
        self.arg = (t[1] << 24) | (t[2] << 16) | (t[3] << 8) | t[4]
        self.ss_bit, self.es_bit = tb(4, 7)[1], tb(1, 0)[2]
        self.putb([134, ['Argument: 0x%04x' % self.arg]])

        # Bits[7:1]: CRC7
        # TODO: Check CRC7.
        crc = t[5] >> 1
        self.ss_bit, self.es_bit = tb(0, 7)[1], tb(0, 1)[2]
        self.putb([134, ['CRC7: 0x%01x' % crc]])

        # Bits[0:0]: End bit (always 1)
        bit, self.ss_bit, self.es_bit = tb(0, 0)[0], tb(0, 0)[1], tb(0, 0)[2]
        self.putb([134, ['End bit: %d' % bit]])
        if bit == 1:
            self.putb([134, ['End bit: %d' % bit]])
        else:
            self.putb([135, ['End bit: %d (Warning: Must be 1!)' % bit]])

        # Handle command.
        if cmd in (0, 1, 9, 16, 17, 41, 49, 55, 59):
            self.state = 'HANDLE CMD%d' % cmd
            self.cmd_str = '%s%d (%s)' % (s, cmd, self.cmd_name(cmd))
        else:
            self.state = 'HANDLE CMD999'
            a = '%s%d: %02x %02x %02x %02x %02x %02x' % ((s, cmd) + tuple(t))
            self.putx([cmd, [a]])

    def handle_cmd0(self):
        # CMD0: GO_IDLE_STATE
        self.putc(0, 'Reset the SD card')
        self.state = 'GET RESPONSE R1'

    def handle_cmd1(self):
        # CMD1: SEND_OP_COND
        self.putc(1, 'Send HCS info and activate the card init process')
        hcs = (self.arg & (1 << 30)) >> 30
        self.ss_bit = self.cmd_token_bits[5 - 4][6][1]
        self.es_bit = self.cmd_token_bits[5 - 4][6][2]
        self.putb([134, ['HCS: %d' % hcs]])
        self.state = 'GET RESPONSE R1'

    def handle_cmd9(self):
        # CMD9: SEND_CSD (128 bits / 16 bytes)
        self.putc(9, 'Ask card to send its card specific data (CSD)')
        if len(self.read_buf) == 0:
            self.ss_cmd = self.ss
        self.read_buf.append(self.miso)
        # FIXME
        ### if len(self.read_buf) < 16:
        if len(self.read_buf) < 16 + 4:
            return
        self.es_cmd = self.es
        self.read_buf = self.read_buf[4:] # TODO: Document or redo.
        self.putx([9, ['CSD: %s' % self.read_buf]])
        # TODO: Decode all bits.
        self.read_buf = []
        ### self.state = 'GET RESPONSE R1'
        self.state = 'IDLE'

    def handle_cmd10(self):
        # CMD10: SEND_CID (128 bits / 16 bytes)
        self.putc(10, 'Ask card to send its card identification (CID)')
        self.read_buf.append(self.miso)
        if len(self.read_buf) < 16:
            return
        self.putx([10, ['CID: %s' % self.read_buf]])
        # TODO: Decode all bits.
        self.read_buf = []
        self.state = 'GET RESPONSE R1'

    def handle_cmd16(self):
        # CMD16: SET_BLOCKLEN
        self.blocklen = self.arg
        # TODO: Sanity check on block length.
        self.putc(16, 'Set the block length to %d bytes' % self.blocklen)
        self.state = 'GET RESPONSE R1'

    def handle_cmd17(self):
        # CMD17: READ_SINGLE_BLOCK
        self.putc(17, 'Read a block from address 0x%04x' % self.arg)
        if len(self.read_buf) == 0:
            self.ss_cmd = self.ss
        self.read_buf.append(self.miso)
        if len(self.read_buf) < self.blocklen + 2: # FIXME
            return
        self.es_cmd = self.es
        self.read_buf = self.read_buf[2:] # FIXME
        self.putx([17, ['Block data: %s' % self.read_buf]])
        self.read_buf = []
        self.state = 'GET RESPONSE R1'

    def handle_cmd49(self):
        self.state = 'GET RESPONSE R1'

    def handle_cmd55(self):
        # CMD55: APP_CMD
        self.putc(55, 'Next command is an application-specific command')
        self.is_acmd = True
        self.state = 'GET RESPONSE R1'

    def handle_cmd59(self):
        # CMD59: CRC_ON_OFF
        crc_on_off = self.arg & (1 << 0)
        s = 'on' if crc_on_off == 1 else 'off'
        self.putc(59, 'Turn the SD card CRC option %s' % s)
        self.state = 'GET RESPONSE R1'

    def handle_acmd41(self):
        # ACMD41: SD_SEND_OP_COND
        self.putc(64 + 41, 'Send HCS info and activate the card init process')
        self.state = 'GET RESPONSE R1'

    def handle_cmd999(self):
        self.state = 'GET RESPONSE R1'

    def handle_cid_register(self):
        # Card Identification (CID) register, 128bits

        cid = self.cid

        # Manufacturer ID: CID[127:120] (8 bits)
        mid = cid[15]

        # OEM/Application ID: CID[119:104] (16 bits)
        oid = (cid[14] << 8) | cid[13]

        # Product name: CID[103:64] (40 bits)
        pnm = 0
        for i in range(12, 8 - 1, -1):
            pnm <<= 8
            pnm |= cid[i]

        # Product revision: CID[63:56] (8 bits)
        prv = cid[7]

        # Product serial number: CID[55:24] (32 bits)
        psn = 0
        for i in range(6, 3 - 1, -1):
            psn <<= 8
            psn |= cid[i]

        # RESERVED: CID[23:20] (4 bits)

        # Manufacturing date: CID[19:8] (12 bits)
        # TODO

        # CRC7 checksum: CID[7:1] (7 bits)
        # TODO

        # Not used, always 1: CID[0:0] (1 bit)
        # TODO

    def handle_response_r1(self, res):
        # The R1 response token format (1 byte).
        # Sent by the card after every command except for SEND_STATUS.

        self.ss_cmd, self.es_cmd = self.miso_bits[7][1], self.miso_bits[0][2]
        self.putx([65, ['R1: 0x%02x' % res]])

        def putbit(bit, data):
            b = self.miso_bits[bit]
            self.ss_bit, self.es_bit = b[1], b[2]
            self.putb([134, data])

        # Bit 0: 'In idle state' bit
        s = '' if (res & (1 << 0)) else 'not '
        putbit(0, ['Card is %sin idle state' % s])

        # Bit 1: 'Erase reset' bit
        s = '' if (res & (1 << 1)) else 'not '
        putbit(1, ['Erase sequence %scleared' % s])

        # Bit 2: 'Illegal command' bit
        s = 'I' if (res & (1 << 2)) else 'No i'
        putbit(2, ['%sllegal command detected' % s])

        # Bit 3: 'Communication CRC error' bit
        s = 'failed' if (res & (1 << 3)) else 'was successful'
        putbit(3, ['CRC check of last command %s' % s])

        # Bit 4: 'Erase sequence error' bit
        s = 'E' if (res & (1 << 4)) else 'No e'
        putbit(4, ['%srror in the sequence of erase commands' % s])

        # Bit 5: 'Address error' bit
        s = 'M' if (res & (1 << 4)) else 'No m'
        putbit(5, ['%sisaligned address used in command' % s])

        # Bit 6: 'Parameter error' bit
        s = '' if (res & (1 << 4)) else 'not '
        putbit(6, ['Command argument %soutside allowed range' % s])

        # Bit 7: Always set to 0
        putbit(7, ['Bit 7 (always 0)'])

        self.state = 'IDLE'

    def handle_response_r1b(self, res):
        # TODO
        pass

    def handle_response_r2(self, res):
        # TODO
        pass

    def handle_response_r3(self, res):
        # TODO
        pass

    # Note: Response token formats R4 and R5 are reserved for SDIO.

    # TODO: R6?

    def handle_response_r7(self, res):
        # TODO
        pass

    def decode(self, ss, es, data):
        ptype, mosi, miso = data

        # For now, only use DATA and BITS packets.
        if ptype not in ('DATA', 'BITS'):
            return

        # Store the individual bit values and ss/es numbers. The next packet
        # is guaranteed to be a 'DATA' packet belonging to this 'BITS' one.
        if ptype == 'BITS':
            self.miso_bits, self.mosi_bits = miso, mosi
            return

        self.ss, self.es = ss, es

        # State machine.
        if self.state == 'IDLE':
            # Ignore stray 0xff bytes, some devices seem to send those!?
            if mosi == 0xff: # TODO?
                return
            self.state = 'GET COMMAND TOKEN'
            self.handle_command_token(mosi, miso)
        elif self.state == 'GET COMMAND TOKEN':
            self.handle_command_token(mosi, miso)
        elif self.state.startswith('HANDLE CMD'):
            self.miso, self.mosi = miso, mosi
            # Call the respective handler method for the command.
            a, cmdstr = 'a' if self.is_acmd else '', self.state[10:].lower()
            handle_cmd = getattr(self, 'handle_%scmd%s' % (a, cmdstr))
            handle_cmd()
            self.cmd_token = []
            self.cmd_token_bits = []
            # Leave ACMD mode again after the first command after CMD55.
            if self.is_acmd and cmdstr != '55':
                self.is_acmd = False
        elif self.state.startswith('GET RESPONSE'):
            # Ignore stray 0xff bytes, some devices seem to send those!?
            if miso == 0xff: # TODO?
                return

            # Call the respective handler method for the response.
            s = 'handle_response_%s' % self.state[13:].lower()
            handle_response = getattr(self, s)
            handle_response(miso)

            self.state = 'IDLE'
