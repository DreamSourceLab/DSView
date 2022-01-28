##
## This file is part of the libsigrokdecode project.
##
## Copyright (C) 2012-2013 Uwe Hermann <uwe@hermann-uwe.de>
## Copyright (C) 2019 DreamSourceLab <support@dreamsourcelab.com>
## Copyright (C) 2020 Raptor Engineering, LLC <support@raptorengineering.com>
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

# ...
fields = {
    # START field (indicates start or stop of a transaction)
    'START': {
        0b0000: 'Start of cycle for a target',
        0b0001: 'Reserved',
        0b0010: 'Grant for bus master 0',
        0b0011: 'Grant for bus master 1',
        0b0100: 'Reserved',
        0b0101: 'TPM',
        0b0110: 'Reserved',
        0b0111: 'Reserved',
        0b1000: 'Reserved',
        0b1001: 'Reserved',
        0b1010: 'Reserved',
        0b1011: 'Reserved',
        0b1100: 'Reserved',
        0b1101: 'Start of cycle for a Firmware Memory Read cycle',
        0b1110: 'Start of cycle for a Firmware Memory Write cycle',
        0b1111: 'Stop/abort (end of a cycle for a target)',
    },
    # Cycle type / direction field
    # Bit 0 (LAD[0]) is unused, should always be 0.
    # Neither host nor peripheral are allowed to drive 0b11x0.
    'CT_DR': {
        0b0000: 'I/O read',
        0b0001: 'I/O read',
        0b0010: 'I/O write',
        0b0011: 'I/O write',
        0b0100: 'Memory read',
        0b0101: 'Memory read',
        0b0110: 'Memory write',
        0b0111: 'Memory write',
        0b1000: 'DMA read',
        0b1001: 'DMA read',
        0b1010: 'DMA write',
        0b1011: 'DMA write',
        0b1100: 'Reserved / not allowed',
        0b1101: 'Reserved / not allowed',
        0b1110: 'Reserved / not allowed',
        0b1111: 'Reserved / not allowed',
    },
    # Cycle type / direction field
    # False for read cycle, True for write cycle
    'CT_DR_WR': {
        0b0000: False,
        0b0001: False,
        0b0010: True,
        0b0011: True,
        0b0100: False,
        0b0101: False,
        0b0110: True,
        0b0111: True,
        0b1000: False,
        0b1001: False,
        0b1010: True,
        0b1011: True,
        0b1100: False,
        0b1101: False,
        0b1110: False,
        0b1111: False,
    },
    # SIZE field (determines how many bytes are to be transferred)
    # Bits[3:2] are reserved, must be driven to 0b00.
    # Neither host nor peripheral are allowed to drive 0b0010.
    'SIZE': {
        0b0000: '8 bits (1 byte)',
        0b0001: '16 bits (2 bytes)',
        0b0010: 'Reserved / not allowed',
        0b0011: '32 bits (4 bytes)',
    },
    # CHANNEL field (bits[2:0] contain the DMA channel number)
    'CHANNEL': {
        0b0000: '0',
        0b0001: '1',
        0b0010: '2',
        0b0011: '3',
        0b0100: '4',
        0b0101: '5',
        0b0110: '6',
        0b0111: '7',
    },
    # SYNC field (used to add wait states)
    'SYNC': {
        0b0000: 'Ready',
        0b0001: 'Reserved',
        0b0010: 'Reserved',
        0b0011: 'Reserved',
        0b0100: 'Reserved',
        0b0101: 'Short wait',
        0b0110: 'Long wait',
        0b0111: 'Reserved',
        0b1000: 'Reserved',
        0b1001: 'Ready more (DMA only)',
        0b1010: 'Error',
        0b1011: 'Reserved',
        0b1100: 'Reserved',
        0b1101: 'Reserved',
        0b1110: 'Reserved',
        0b1111: 'Reserved',
    },
}

class Decoder(srd.Decoder):
    api_version = 3
    id = 'lpc'
    name = 'LPC'
    longname = 'Low Pin Count'
    desc = 'Protocol for low-bandwidth devices on PC mainboards.'
    license = 'gplv2+'
    inputs = ['logic']
    outputs = []
    tags = ['PC']
    channels = (
        {'id': 'lframe', 'name': 'LFRAME#', 'desc': 'Frame'},
        {'id': 'lclk',   'name': 'LCLK',    'desc': 'Clock'},
        {'id': 'lad0',   'name': 'LAD[0]',  'desc': 'Addr/control/data 0'},
        {'id': 'lad1',   'name': 'LAD[1]',  'desc': 'Addr/control/data 1'},
        {'id': 'lad2',   'name': 'LAD[2]',  'desc': 'Addr/control/data 2'},
        {'id': 'lad3',   'name': 'LAD[3]',  'desc': 'Addr/control/data 3'},
    )
    optional_channels = (
        {'id': 'lreset', 'name': 'LRESET#', 'desc': 'Reset'},
        {'id': 'ldrq',   'name': 'LDRQ#',   'desc': 'Encoded DMA / bus master request'},
        {'id': 'serirq', 'name': 'SERIRQ',  'desc': 'Serialized IRQ'},
        {'id': 'clkrun', 'name': 'CLKRUN#', 'desc': 'Clock run'},
        {'id': 'lpme',   'name': 'LPME#',   'desc': 'LPC power management event'},
        {'id': 'lpcpd',  'name': 'LPCPD#',  'desc': 'Power down'},
        {'id': 'lsmi',   'name': 'LSMI#',   'desc': 'System Management Interrupt'},
    )
    annotations = (
        ('warnings', 'Warnings'),
        ('start', 'Start'),
        ('cycle-type', 'Cycle-type/direction'),
        ('addr', 'Address'),
        ('tar1', 'Turn-around cycle 1'),
        ('sync', 'Sync'),
        ('timeout', 'Time Out'),
        ('data', 'Data'),
        ('tar2', 'Turn-around cycle 2'),
    )
    annotation_rows = (
        ('data', 'Data', (1, 2, 3, 4, 5, 6, 7, 8)),
        ('warnings', 'Warnings', (0,)),
    )

    def __init__(self):
        self.reset()

    def reset(self):
        self.state = 'IDLE'
        self.oldlclk = -1
        self.samplenum = 0
        self.lad = -1
        self.addr = 0
        self.direction = 0
        self.cur_nibble = 0
        self.cycle_type = -1
        self.databyte = 0
        self.tarcount = 0
        self.synccount = 0
        self.timeoutcount = 0
        self.oldpins = None
        self.ss_block = self.es_block = None

    def start(self):
        self.out_ann = self.register(srd.OUTPUT_ANN)

    def putb(self, data):
        self.put(self.ss_block, self.es_block, self.out_ann, data)

    def handle_get_start(self, lframe):
        # LAD[3:0]: START field (1 clock cycle).

        # The last value of LAD[3:0] before LFRAME# gets de-asserted is what
        # the peripherals must use. However, the host can keep LFRAME# asserted
        # multiple clocks, and we output all START fields that occur, even
        # though the peripherals are supposed to ignore all but the last one.
        self.es_block = self.samplenum
        self.putb([1, [fields['START'][self.oldlad], 'START', 'St', 'S']])
        self.ss_block = self.samplenum


        # LFRAME# is asserted (low). Wait until it gets de-asserted again
        # (the host is allowed to keep it asserted multiple clocks).
        if lframe != 1:
            return

        if (self.oldlad == 0b0000 or self.oldlad == 0b0101):
            self.start_field = self.oldlad
            self.state = 'GET CT/DR'
        elif (self.oldlad == 0b1101 or self.oldlad == 0b1110):
            self.start_field = self.oldlad
            if (self.oldlad == 0b1110):
                self.direction = True
            else:
                self.direction = False
            self.state = 'GET FW IDSEL'
        else:
            self.state = 'IDLE'

    def handle_get_ct_dr(self):
        # LAD[3:0]: Cycle type / direction field (1 clock cycle).

        self.cycle_type = fields['CT_DR'][self.oldlad]
        self.direction = fields['CT_DR_WR'][self.oldlad]

        # TODO: Warning/error on invalid cycle types.
        if self.cycle_type == 'Reserved':
            self.putb([0, ['Invalid cycle type (%s)' % self.oldlad_bits]])

        self.es_block = self.samplenum
        self.putb([2, ['Cycle type: %s' % self.cycle_type]])
        self.ss_block = self.samplenum

        self.state = 'GET ADDR'
        self.addr = 0
        self.cur_nibble = 0

    def handle_get_fw_idsel(self):
        # LAD[3:0]: IDSEL field (1 clock cycle).
        self.es_block = self.samplenum
        s = 'IDSEL: 0x%%0%dx' % self.oldlad
        self.putb([3, [s % self.oldlad]])
        self.ss_block = self.samplenum

        self.state = 'GET FW ADDR'
        self.addr = 0
        self.cur_nibble = 0

    def handle_get_fw_addr(self):
        # LAD[3:0]: ADDR field (7 clock cycles).
        addr_nibbles = 7 # Address is 28bits.

        # Addresses are driven MSN-first.
        offset = ((addr_nibbles - 1) - self.cur_nibble) * 4
        if (offset < 0):
            self.putb([0, ['Warning: Invalid address shift: %d' % offset]])
            self.state = 'IDLE'
            return
        self.addr |= (self.oldlad << offset)

        # Continue if we haven't seen all ADDR cycles, yet.
        if (self.cur_nibble < addr_nibbles - 1):
            self.cur_nibble += 1
            return

        self.es_block = self.samplenum
        s = 'Address: 0x%%0%dx' % addr_nibbles
        self.putb([3, [s % self.addr]])
        self.ss_block = self.samplenum

        self.state = 'GET FW MSIZE'

    def handle_get_fw_msize(self):
        # LAD[3:0]: MSIZE field (1 clock cycle).
        self.es_block = self.samplenum
        s = 'MSIZE: 0x%%0%dx' % self.oldlad
        self.putb([3, [s % self.oldlad]])
        self.ss_block = self.samplenum
        self.msize = self.oldlad

        if self.direction == 1:
            self.state = 'GET FW DATA'
            self.cycle_count = 0
            self.dataword = 0
            self.cur_nibble = 0
        else:
            self.state = 'GET TAR'
            self.tar_count = 0

    def handle_get_addr(self):
        # LAD[3:0]: ADDR field (4/8/0 clock cycles).

        # I/O cycles: 4 ADDR clocks. Memory cycles: 8 ADDR clocks.
        # DMA cycles: no ADDR clocks at all.
        if self.cycle_type in ('I/O read', 'I/O write'):
            addr_nibbles = 4 # Address is 16bits.
        elif self.cycle_type in ('Memory read', 'Memory write'):
            addr_nibbles = 8 # Address is 32bits.
        else:
            addr_nibbles = 0 # TODO: How to handle later on?

        # Addresses are driven MSN-first.
        offset = ((addr_nibbles - 1) - self.cur_nibble) * 4
        if (offset < 0):
            self.putb([0, ['Warning: Invalid address shift: %d' % offset]])
            self.state = 'IDLE'
            return
        self.addr |= (self.oldlad << offset)

        # Continue if we haven't seen all ADDR cycles, yet.
        if (self.cur_nibble < addr_nibbles - 1):
            self.cur_nibble += 1
            return

        self.es_block = self.samplenum
        s = 'Address: 0x%%0%dx' % addr_nibbles
        self.putb([3, [s % self.addr]])
        self.ss_block = self.samplenum

        if self.direction == 1:
            self.state = 'GET DATA'
            self.cycle_count = 0
        else:
            self.state = 'GET TAR'
            self.tar_count = 0

    def handle_get_tar(self):
        # LAD[3:0]: First TAR (turn-around) field (2 clock cycles).

        self.es_block = self.samplenum
        self.putb([4, ['TAR, cycle %d: %s' % (self.tarcount, self.oldlad_bits)]])
        self.ss_block = self.samplenum

        # On the first TAR clock cycle LAD[3:0] is driven to 1111 by
        # either the host or peripheral. On the second clock cycle,
        # the host or peripheral tri-states LAD[3:0], but its value
        # should still be 1111, due to pull-ups on the LAD lines.
        if self.oldlad_bits != '1111':
            self.putb([0, ['TAR, cycle %d: %s (expected 1111)' % \
                           (self.tarcount, self.oldlad_bits)]])

        if (self.tarcount != 1):
            self.tarcount += 1
            return

        self.tarcount = 0
        self.state = 'GET SYNC'

    def handle_get_sync(self, lframe):
        # LAD[3:0]: SYNC field (1-n clock cycles).

        self.sync_val = self.oldlad_bits
        self.cycle_type = fields['SYNC'][self.oldlad]

        self.es_block = self.samplenum
        # TODO: Warnings if reserved value are seen?
        if self.cycle_type == 'Reserved':
            self.putb([0, ['SYNC, cycle %d: %s (reserved value)' % \
                           (self.synccount, self.sync_val)]])

        self.es_block = self.samplenum
        self.putb([5, ['SYNC, cycle %d: %s' % (self.synccount, self.sync_val)]])
        self.ss_block = self.samplenum

        # TODO
        if (self.cycle_type != 'Short wait' and self.cycle_type != 'Long wait'):
            self.cycle_count = 0
            if (lframe == 0):
                self.state = 'GET TIMEOUT'
            elif (self.start_field == 0b1101 or self.start_field == 0b1110):
                self.state = 'GET FW DATA'
                self.cycle_count = 0
                self.dataword = 0
                self.cur_nibble = 0
            else:
                self.state = 'GET DATA'

    def handle_get_timeout(self):
        # LFRAME#: tie low (4 clock cycles).

        if (self.oldlframe != 0):
            self.putb([0, ['TIMEOUT cycle, LFRAME# must be low for 4 LCLk cycles']])
            self.timeoutcount = 0
            self.state = 'IDLE'
            return

        self.es_block = self.samplenum
        self.putb([6, ['Timeout %d' % self.timeoutcount]])
        self.ss_block = self.samplenum

        if (self.timeoutcount != 3):
            self.timeoutcount += 1
            return

        self.timeoutcount = 0
        self.state = 'IDLE'

    def handle_get_fw_data(self):
        # LAD[3:0]: DATA field
        if (self.msize == 0b0000):
            data_nibbles = 2 # Data is 8bits.
        elif (self.msize == 0b0001):
            data_nibbles = 4 # Data is 16bits.
        elif (self.msize == 0b0010):
            data_nibbles = 8 # Data is 32bits.
        elif (self.msize == 0b0100):
            data_nibbles = 32 # Data is 128bits.
        elif (self.msize == 0b0111):
            data_nibbles = 256 # Data is 1024bits.
        else:
            self.putb([0, ['Warning: Invalid MSIZE: %d' % self.msize]])
            self.state = 'IDLE'
            return

        # Data is driven LSN-first.
        nibble_swap = self.cur_nibble % 2
        offset = ((data_nibbles - 1) - self.cur_nibble) * 4
        if (nibble_swap):
            offset += 4
        else:
            offset -= 4
        if (offset < 0):
            self.putb([0, ['Warning: Invalid data shift: %d' % offset]])
            self.state = 'IDLE'
            return
        self.dataword |= (self.oldlad << offset)

        # Continue if we haven't seen all DATA cycles, yet.
        if (self.cur_nibble < data_nibbles - 1):
            self.cur_nibble += 1
            return

        self.es_block = self.samplenum
        s = 'DATA: 0x%%0%dx' % data_nibbles
        self.putb([3, [s % self.dataword]])
        self.ss_block = self.samplenum

        self.cycle_count = 0
        self.state = 'GET TAR2'

    def handle_get_data(self):
        # LAD[3:0]: DATA field (2 clock cycles).

        # Data is driven LSN-first.
        if (self.cycle_count == 0):
            self.databyte = self.oldlad
        elif (self.cycle_count == 1):
            self.databyte |= (self.oldlad << 4)
        else:
            self.putb([0, ['Warning: Invalid cycle_count: %d' % self.cycle_count]])
            self.state = 'IDLE'
            return

        if (self.cycle_count != 1):
            self.cycle_count += 1
            return

        self.es_block = self.samplenum
        self.putb([7, ['DATA: 0x%02x' % self.databyte]])
        self.ss_block = self.samplenum

        self.cycle_count = 0
        self.state = 'GET TAR2'

    def handle_get_tar2(self):
        # LAD[3:0]: Second TAR field (2 clock cycles).

        self.es_block = self.samplenum
        self.putb([8, ['TAR, cycle %d: %s' % (self.tarcount, self.oldlad_bits)]])
        self.ss_block = self.samplenum

        # On the first TAR clock cycle LAD[3:0] is driven to 1111 by
        # either the host or peripheral. On the second clock cycle,
        # the host or peripheral tri-states LAD[3:0], but its value
        # should still be 1111, due to pull-ups on the LAD lines.
        if self.oldlad_bits != '1111':
            self.putb([0, ['Warning: TAR, cycle %d: %s (expected 1111)'
                           % (self.tarcount, self.oldlad_bits)]])

        if (self.tarcount != 1):
            self.tarcount += 1
            return

        self.tarcount = 0
        self.state = 'IDLE'

    def decode(self):
        while True:

            # Only look at the signals upon rising LCLK edges. The LPC clock
            # is the same as the PCI clock (which is sampled at rising edges).
            (lframe, lclk, lad0, lad1, lad2, lad3, lreset, ldrq, serirq, clkrun, lpme, lpcpd, lsmi) = self.wait({1: 'r'})

            # Store LAD[3:0] bit values (one nibble) in local variables.
            # Most (but not all) states need this.
            lad = (lad3 << 3) | (lad2 << 2) | (lad1 << 1) | lad0
            lad_bits = '{:04b}'.format(lad)
            # self.putb([0, ['LAD: %s' % lad_bits]])

            # TODO: Only memory read/write is currently supported/tested.

            # Detect host cycle abort requests
            if (lframe == 0) and (self.oldlframe == 0):
                self.state = 'GET TIMEOUT'

            # State machine
            if self.state == 'IDLE':
                # A valid LPC cycle starts with LFRAME# being asserted (low).
                if lframe == 0:
                    self.ss_block = self.samplenum
                    self.state = 'GET START'
                    self.lad = -1
                else:
                    self.wait({0: 'f'})
            elif self.state == 'GET START':
                self.handle_get_start(lframe)
            elif self.state == 'GET CT/DR':
                self.handle_get_ct_dr()
            elif self.state == 'GET ADDR':
                self.handle_get_addr()
            elif self.state == 'GET FW IDSEL':
                self.handle_get_fw_idsel()
            elif self.state == 'GET FW ADDR':
                self.handle_get_fw_addr()
            elif self.state == 'GET FW MSIZE':
                self.handle_get_fw_msize()
            elif self.state == 'GET TAR':
                self.handle_get_tar()
            elif self.state == 'GET SYNC':
                self.handle_get_sync(lframe)
            elif self.state == 'GET TIMEOUT':
                self.handle_get_timeout()
            elif self.state == 'GET FW DATA':
                self.handle_get_fw_data()
            elif self.state == 'GET DATA':
                self.handle_get_data()
            elif self.state == 'GET TAR2':
                self.handle_get_tar2()

            self.oldlframe = lframe
            self.oldlad = lad
            self.oldlad_bits = lad_bits
