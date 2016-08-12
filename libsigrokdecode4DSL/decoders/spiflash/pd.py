##
## This file is part of the libsigrokdecode project.
##
## Copyright (C) 2011-2015 Uwe Hermann <uwe@hermann-uwe.de>
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

def cmd_annotation_classes():
    return tuple([tuple([cmd[0].lower(), cmd[1]]) for cmd in cmds.values()])

def decode_status_reg(data):
    # TODO: Additional per-bit(s) self.put() calls with correct start/end.

    # Bits[0:0]: WIP (write in progress)
    s = 'W' if (data & (1 << 0)) else 'No w'
    ret = '%srite operation in progress.\n' % s

    # Bits[1:1]: WEL (write enable latch)
    s = '' if (data & (1 << 1)) else 'not '
    ret += 'Internal write enable latch is %sset.\n' % s

    # Bits[5:2]: Block protect bits
    # TODO: More detailed decoding (chip-dependent).
    ret += 'Block protection bits (BP3-BP0): 0x%x.\n' % ((data & 0x3c) >> 2)

    # Bits[6:6]: Continuously program mode (CP mode)
    s = '' if (data & (1 << 6)) else 'not '
    ret += 'Device is %sin continuously program mode (CP mode).\n' % s

    # Bits[7:7]: SRWD (status register write disable)
    s = 'not ' if (data & (1 << 7)) else ''
    ret += 'Status register writes are %sallowed.\n' % s

    return ret

class Decoder(srd.Decoder):
    api_version = 2
    id = 'spiflash'
    name = 'SPI flash'
    longname = 'SPI flash chips'
    desc = 'xx25 series SPI (NOR) flash chip protocol.'
    license = 'gplv2+'
    inputs = ['spi']
    outputs = ['spiflash']
    annotations = cmd_annotation_classes() + (
        ('bits', 'Bits'),
        ('bits2', 'Bits2'),
        ('warnings', 'Warnings'),
    )
    annotation_rows = (
        ('bits', 'Bits', (24, 25)),
        ('commands', 'Commands', tuple(range(23 + 1))),
        ('warnings', 'Warnings', (26,)),
    )
    options = (
        {'id': 'chip', 'desc': 'Chip', 'default': tuple(chips.keys())[0],
            'values': tuple(chips.keys())},
    )

    def __init__(self):
        self.state = None
        self.cmdstate = 1
        self.addr = 0
        self.data = []

    def start(self):
        self.out_ann = self.register(srd.OUTPUT_ANN)
        self.chip = chips[self.options['chip']]

    def putx(self, data):
        # Simplification, most annotations span exactly one SPI byte/packet.
        self.put(self.ss, self.es, self.out_ann, data)

    def putb(self, data):
        self.put(self.block_ss, self.block_es, self.out_ann, data)

    def handle_wren(self, mosi, miso):
        self.putx([0, ['Command: %s' % cmds[self.state][1]]])
        self.state = None

    def handle_wrdi(self, mosi, miso):
        pass # TODO

    # TODO: Check/display device ID / name
    def handle_rdid(self, mosi, miso):
        if self.cmdstate == 1:
            # Byte 1: Master sends command ID.
            self.ss_block = self.ss
            self.putx([2, ['Command: %s' % cmds[self.state][1]]])
        elif self.cmdstate == 2:
            # Byte 2: Slave sends the JEDEC manufacturer ID.
            self.putx([2, ['Manufacturer ID: 0x%02x' % miso]])
        elif self.cmdstate == 3:
            # Byte 3: Slave sends the memory type (0x20 for this chip).
            self.putx([2, ['Memory type: 0x%02x' % miso]])
        elif self.cmdstate == 4:
            # Byte 4: Slave sends the device ID.
            self.device_id = miso
            self.putx([2, ['Device ID: 0x%02x' % miso]])

        if self.cmdstate == 4:
            # TODO: Check self.device_id is valid & exists in device_names.
            # TODO: Same device ID? Check!
            d = 'Device: Macronix %s' % device_name[self.device_id]
            self.put(self.ss_block, self.es, self.out_ann, [0, [d]])
            self.state = None
        else:
            self.cmdstate += 1

    def handle_rdsr(self, mosi, miso):
        # Read status register: Master asserts CS#, sends RDSR command,
        # reads status register byte. If CS# is kept asserted, the status
        # register can be read continuously / multiple times in a row.
        # When done, the master de-asserts CS# again.
        if self.cmdstate == 1:
            # Byte 1: Master sends command ID.
            self.putx([3, ['Command: %s' % cmds[self.state][1]]])
        elif self.cmdstate >= 2:
            # Bytes 2-x: Slave sends status register as long as master clocks.
            if self.cmdstate <= 3: # TODO: While CS# asserted.
                self.putx([24, ['Status register: 0x%02x' % miso]])
                self.putx([25, [decode_status_reg(miso)]])

            if self.cmdstate == 3: # TODO: If CS# got de-asserted.
                self.state = None
                return

        self.cmdstate += 1

    def handle_wrsr(self, mosi, miso):
        pass # TODO

    def handle_read(self, mosi, miso):
        # Read data bytes: Master asserts CS#, sends READ command, sends
        # 3-byte address, reads >= 1 data bytes, de-asserts CS#.
        if self.cmdstate == 1:
            # Byte 1: Master sends command ID.
            self.putx([5, ['Command: %s' % cmds[self.state][1]]])
        elif self.cmdstate in (2, 3, 4):
            # Bytes 2/3/4: Master sends read address (24bits, MSB-first).
            self.addr |= (mosi << ((4 - self.cmdstate) * 8))
            # self.putx([0, ['Read address, byte %d: 0x%02x' % \
            #                (4 - self.cmdstate, mosi)]])
            if self.cmdstate == 4:
                self.putx([24, ['Read address: 0x%06x' % self.addr]])
                self.addr = 0
        elif self.cmdstate >= 5:
            # Bytes 5-x: Master reads data bytes (until CS# de-asserted).
            # TODO: For now we hardcode 256 bytes per READ command.
            if self.cmdstate <= 256 + 4: # TODO: While CS# asserted.
                self.data.append(miso)
                # self.putx([0, ['New read byte: 0x%02x' % miso]])

            if self.cmdstate == 256 + 4: # TODO: If CS# got de-asserted.
                # s = ', '.join(map(hex, self.data))
                s = ''.join(map(chr, self.data))
                self.putx([24, ['Read data']])
                self.putx([25, ['Read data: %s' % s]])
                self.data = []
                self.state = None
                return

        self.cmdstate += 1

    def handle_fast_read(self, mosi, miso):
        # Fast read: Master asserts CS#, sends FAST READ command, sends
        # 3-byte address + 1 dummy byte, reads >= 1 data bytes, de-asserts CS#.
        if self.cmdstate == 1:
            # Byte 1: Master sends command ID.
            self.putx([5, ['Command: %s' % cmds[self.state][1]]])
        elif self.cmdstate in (2, 3, 4):
            # Bytes 2/3/4: Master sends read address (24bits, MSB-first).
            self.putx([24, ['AD%d: 0x%02x' % (self.cmdstate - 1, mosi)]])
            if self.cmdstate == 2:
                self.block_ss = self.ss
            self.addr |= (mosi << ((4 - self.cmdstate) * 8))
        elif self.cmdstate == 5:
            self.putx([24, ['Dummy byte: 0x%02x' % mosi]])
            self.block_es = self.es
            self.putb([5, ['Read address: 0x%06x' % self.addr]])
            self.addr = 0
        elif self.cmdstate >= 6:
            # Bytes 6-x: Master reads data bytes (until CS# de-asserted).
            # TODO: For now we hardcode 32 bytes per FAST READ command.
            if self.cmdstate == 6:
                self.block_ss = self.ss
            if self.cmdstate <= 32 + 5: # TODO: While CS# asserted.
                self.data.append(miso)
            if self.cmdstate == 32 + 5: # TODO: If CS# got de-asserted.
                self.block_es = self.es
                s = ' '.join([hex(b)[2:] for b in self.data])
                self.putb([25, ['Read data: %s' % s]])
                self.data = []
                self.state = None
                return

        self.cmdstate += 1

    def handle_2read(self, mosi, miso):
        pass # TODO

    # TODO: Warn/abort if we don't see the necessary amount of bytes.
    # TODO: Warn if WREN was not seen before.
    def handle_se(self, mosi, miso):
        if self.cmdstate == 1:
            # Byte 1: Master sends command ID.
            self.addr = 0
            self.ss_block = self.ss
            self.putx([8, ['Command: %s' % cmds[self.state][1]]])
        elif self.cmdstate in (2, 3, 4):
            # Bytes 2/3/4: Master sends sector address (24bits, MSB-first).
            self.addr |= (mosi << ((4 - self.cmdstate) * 8))
            # self.putx([0, ['Sector address, byte %d: 0x%02x' % \
            #                (4 - self.cmdstate, mosi)]])

        if self.cmdstate == 4:
            d = 'Erase sector %d (0x%06x)' % (self.addr, self.addr)
            self.put(self.ss_block, self.es, self.out_ann, [24, [d]])
            # TODO: Max. size depends on chip, check that too if possible.
            if self.addr % 4096 != 0:
                # Sector addresses must be 4K-aligned (same for all 3 chips).
                d = 'Warning: Invalid sector address!'
                self.put(self.ss_block, self.es, self.out_ann, [101, [d]])
            self.state = None
        else:
            self.cmdstate += 1

    def handle_be(self, mosi, miso):
        pass # TODO

    def handle_ce(self, mosi, miso):
        pass # TODO

    def handle_ce2(self, mosi, miso):
        pass # TODO

    def handle_pp(self, mosi, miso):
        # Page program: Master asserts CS#, sends PP command, sends 3-byte
        # page address, sends >= 1 data bytes, de-asserts CS#.
        if self.cmdstate == 1:
            # Byte 1: Master sends command ID.
            self.putx([12, ['Command: %s' % cmds[self.state][1]]])
        elif self.cmdstate in (2, 3, 4):
            # Bytes 2/3/4: Master sends page address (24bits, MSB-first).
            self.addr |= (mosi << ((4 - self.cmdstate) * 8))
            # self.putx([0, ['Page address, byte %d: 0x%02x' % \
            #                (4 - self.cmdstate, mosi)]])
            if self.cmdstate == 4:
                self.putx([24, ['Page address: 0x%06x' % self.addr]])
                self.addr = 0
        elif self.cmdstate >= 5:
            # Bytes 5-x: Master sends data bytes (until CS# de-asserted).
            # TODO: For now we hardcode 256 bytes per page / PP command.
            if self.cmdstate <= 256 + 4: # TODO: While CS# asserted.
                self.data.append(mosi)
                # self.putx([0, ['New data byte: 0x%02x' % mosi]])

            if self.cmdstate == 256 + 4: # TODO: If CS# got de-asserted.
                # s = ', '.join(map(hex, self.data))
                s = ''.join(map(chr, self.data))
                self.putx([24, ['Page data']])
                self.putx([25, ['Page data: %s' % s]])
                self.data = []
                self.state = None
                return

        self.cmdstate += 1

    def handle_cp(self, mosi, miso):
        pass # TODO

    def handle_dp(self, mosi, miso):
        pass # TODO

    def handle_rdp_res(self, mosi, miso):
        pass # TODO

    def handle_rems(self, mosi, miso):
        if self.cmdstate == 1:
            # Byte 1: Master sends command ID.
            self.ss_block = self.ss
            self.putx([16, ['Command: %s' % cmds[self.state][1]]])
        elif self.cmdstate in (2, 3):
            # Bytes 2/3: Master sends two dummy bytes.
            # TODO: Check dummy bytes? Check reply from device?
            self.putx([24, ['Dummy byte: %s' % mosi]])
        elif self.cmdstate == 4:
            # Byte 4: Master sends 0x00 or 0x01.
            # 0x00: Master wants manufacturer ID as first reply byte.
            # 0x01: Master wants device ID as first reply byte.
            self.manufacturer_id_first = True if (mosi == 0x00) else False
            d = 'manufacturer' if (mosi == 0x00) else 'device'
            self.putx([24, ['Master wants %s ID first' % d]])
        elif self.cmdstate == 5:
            # Byte 5: Slave sends manufacturer ID (or device ID).
            self.ids = [miso]
            d = 'Manufacturer' if self.manufacturer_id_first else 'Device'
            self.putx([24, ['%s ID' % d]])
        elif self.cmdstate == 6:
            # Byte 6: Slave sends device ID (or manufacturer ID).
            self.ids.append(miso)
            d = 'Manufacturer' if self.manufacturer_id_first else 'Device'
            self.putx([24, ['%s ID' % d]])

        if self.cmdstate == 6:
            id = self.ids[1] if self.manufacturer_id_first else self.ids[0]
            self.putx([24, ['Device: Macronix %s' % device_name[id]]])
            self.state = None
        else:
            self.cmdstate += 1

    def handle_rems2(self, mosi, miso):
        pass # TODO

    def handle_enso(self, mosi, miso):
        pass # TODO

    def handle_exso(self, mosi, miso):
        pass # TODO

    def handle_rdscur(self, mosi, miso):
        pass # TODO

    def handle_wrscur(self, mosi, miso):
        pass # TODO

    def handle_esry(self, mosi, miso):
        pass # TODO

    def handle_dsry(self, mosi, miso):
        pass # TODO

    def decode(self, ss, es, data):

        ptype, mosi, miso = data

        # if ptype == 'DATA':
        #     self.putx([0, ['MOSI: 0x%02x, MISO: 0x%02x' % (mosi, miso)]])

        # if ptype == 'CS-CHANGE':
        #     if mosi == 1 and miso == 0:
        #         self.putx([0, ['Asserting CS#']])
        #     elif mosi == 0 and miso == 1:
        #         self.putx([0, ['De-asserting CS#']])

        if ptype != 'DATA':
            return

        self.ss, self.es = ss, es

        # If we encountered a known chip command, enter the resp. state.
        if self.state is None:
            self.state = mosi
            self.cmdstate = 1

        # Handle commands.
        if self.state in cmds:
            s = 'handle_%s' % cmds[self.state][0].lower().replace('/', '_')
            handle_reg = getattr(self, s)
            handle_reg(mosi, miso)
        else:
            self.putx([24, ['Unknown command: 0x%02x' % mosi]])
            self.state = None
