##
## This file is part of the libsigrokdecode project.
##
## Copyright (C) 2012-2013 Uwe Hermann <uwe@hermann-uwe.de>
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
    id = 'can'
    name = 'CAN'
    longname = 'Controller Area Network'
    desc = 'Field bus protocol for distributed realtime control.'
    license = 'gplv2+'
    inputs = ['logic']
    outputs = ['can']
    channels = (
        {'id': 'can_rx', 'name': 'CAN RX', 'desc': 'CAN bus line'},
    )
    options = (
        {'id': 'bitrate', 'desc': 'Bitrate (bits/s)', 'default': 1000000},
        {'id': 'sample_point', 'desc': 'Sample point (%)', 'default': 70.0},
    )
    annotations = (
        ('data', 'CAN payload data'),
        ('sof', 'Start of frame'),
        ('eof', 'End of frame'),
        ('id', 'Identifier'),
        ('ext-id', 'Extended identifier'),
        ('full-id', 'Full identifier'),
        ('ide', 'Identifier extension bit'),
        ('reserved-bit', 'Reserved bit 0 and 1'),
        ('rtr', 'Remote transmission request'),
        ('srr', 'Substitute remote request'),
        ('dlc', 'Data length count'),
        ('crc-sequence', 'CRC sequence'),
        ('crc-delimiter', 'CRC delimiter'),
        ('ack-slot', 'ACK slot'),
        ('ack-delimiter', 'ACK delimiter'),
        ('stuff-bit', 'Stuff bit'),
        ('warnings', 'Human-readable warnings'),
        ('bit', 'Bit'),
    )
    annotation_rows = (
        ('bits', 'Bits', (15, 17)),
        ('fields', 'Fields', tuple(range(15)) + (16,)),
    )

    def __init__(self):
        self.samplerate = None
        self.reset_variables()

    def start(self):
        self.out_ann = self.register(srd.OUTPUT_ANN)

    def metadata(self, key, value):
        if key == srd.SRD_CONF_SAMPLERATE:
            self.samplerate = value
            self.bit_width = float(self.samplerate) / float(self.options['bitrate'])
            self.bitpos = (self.bit_width / 100.0) * self.options['sample_point']

    # Generic helper for CAN bit annotations.
    def putg(self, ss, es, data):
        left, right = int(self.bitpos), int(self.bit_width - self.bitpos)
        self.put(ss - left, es + right, self.out_ann, data)

    # Single-CAN-bit annotation using the current samplenum.
    def putx(self, data):
        self.putg(self.samplenum, self.samplenum, data)

    # Single-CAN-bit annotation using the samplenum of CAN bit 12.
    def put12(self, data):
        self.putg(self.ss_bit12, self.ss_bit12, data)

    # Multi-CAN-bit annotation from self.ss_block to current samplenum.
    def putb(self, data):
        self.putg(self.ss_block, self.samplenum, data)

    def reset_variables(self):
        self.state = 'IDLE'
        self.sof = self.frame_type = self.dlc = None
        self.rawbits = [] # All bits, including stuff bits
        self.bits = [] # Only actual CAN frame bits (no stuff bits)
        self.curbit = 0 # Current bit of CAN frame (bit 0 == SOF)
        self.last_databit = 999 # Positive value that bitnum+x will never match
        self.ss_block = None
        self.ss_bit12 = None
        self.ss_databytebits = []

    # Return True if we reached the desired bit position, False otherwise.
    def reached_bit(self, bitnum):
        bitpos = int(self.sof + (self.bit_width * bitnum) + self.bitpos)
        if self.samplenum >= bitpos:
            return True
        return False

    def is_stuff_bit(self):
        # CAN uses NRZ encoding and bit stuffing.
        # After 5 identical bits, a stuff bit of opposite value is added.
        last_6_bits = self.rawbits[-6:]
        if last_6_bits not in ([0, 0, 0, 0, 0, 1], [1, 1, 1, 1, 1, 0]):
            return False

        # Stuff bit. Keep it in self.rawbits, but drop it from self.bits.
        self.bits.pop() # Drop last bit.
        return True

    def is_valid_crc(self, crc_bits):
        return True # TODO

    def decode_error_frame(self, bits):
        pass # TODO

    def decode_overload_frame(self, bits):
        pass # TODO

    # Both standard and extended frames end with CRC, CRC delimiter, ACK,
    # ACK delimiter, and EOF fields. Handle them in a common function.
    # Returns True if the frame ended (EOF), False otherwise.
    def decode_frame_end(self, can_rx, bitnum):

        # Remember start of CRC sequence (see below).
        if bitnum == (self.last_databit + 1):
            self.ss_block = self.samplenum

        # CRC sequence (15 bits)
        elif bitnum == (self.last_databit + 15):
            x = self.last_databit + 1
            crc_bits = self.bits[x:x + 15 + 1]
            self.crc = int(''.join(str(d) for d in crc_bits), 2)
            self.putb([11, ['CRC sequence: 0x%04x' % self.crc,
                            'CRC: 0x%04x' % self.crc, 'CRC']])
            if not self.is_valid_crc(crc_bits):
                self.putb([16, ['CRC is invalid']])

        # CRC delimiter bit (recessive)
        elif bitnum == (self.last_databit + 16):
            self.putx([12, ['CRC delimiter: %d' % can_rx,
                            'CRC d: %d' % can_rx, 'CRC d']])

        # ACK slot bit (dominant: ACK, recessive: NACK)
        elif bitnum == (self.last_databit + 17):
            ack = 'ACK' if can_rx == 0 else 'NACK'
            self.putx([13, ['ACK slot: %s' % ack, 'ACK s: %s' % ack, 'ACK s']])

        # ACK delimiter bit (recessive)
        elif bitnum == (self.last_databit + 18):
            self.putx([14, ['ACK delimiter: %d' % can_rx,
                            'ACK d: %d' % can_rx, 'ACK d']])

        # Remember start of EOF (see below).
        elif bitnum == (self.last_databit + 19):
            self.ss_block = self.samplenum

        # End of frame (EOF), 7 recessive bits
        elif bitnum == (self.last_databit + 25):
            self.putb([2, ['End of frame', 'EOF', 'E']])
            self.reset_variables()
            return True

        return False

    # Returns True if the frame ended (EOF), False otherwise.
    def decode_standard_frame(self, can_rx, bitnum):

        # Bit 14: RB0 (reserved bit)
        # Has to be sent dominant, but receivers should accept recessive too.
        if bitnum == 14:
            self.putx([7, ['Reserved bit 0: %d' % can_rx,
                           'RB0: %d' % can_rx, 'RB0']])

            # Bit 12: Remote transmission request (RTR) bit
            # Data frame: dominant, remote frame: recessive
            # Remote frames do not contain a data field.
            rtr = 'remote' if self.bits[12] == 1 else 'data'
            self.put12([8, ['Remote transmission request: %s frame' % rtr,
                            'RTR: %s frame' % rtr, 'RTR']])

        # Remember start of DLC (see below).
        elif bitnum == 15:
            self.ss_block = self.samplenum

        # Bits 15-18: Data length code (DLC), in number of bytes (0-8).
        elif bitnum == 18:
            self.dlc = int(''.join(str(d) for d in self.bits[15:18 + 1]), 2)
            self.putb([10, ['Data length code: %d' % self.dlc,
                            'DLC: %d' % self.dlc, 'DLC']])
            self.last_databit = 18 + (self.dlc * 8)

        # Remember all databyte bits, except the very last one.
        elif bitnum in range(19, self.last_databit):
            self.ss_databytebits.append(self.samplenum)

        # Bits 19-X: Data field (0-8 bytes, depending on DLC)
        # The bits within a data byte are transferred MSB-first.
        elif bitnum == self.last_databit:
            self.ss_databytebits.append(self.samplenum) # Last databyte bit.
            for i in range(self.dlc):
                x = 18 + (8 * i) + 1
                b = int(''.join(str(d) for d in self.bits[x:x + 8]), 2)
                ss = self.ss_databytebits[i * 8]
                es = self.ss_databytebits[((i + 1) * 8) - 1]
                self.putg(ss, es, [0, ['Data byte %d: 0x%02x' % (i, b),
                                       'DB %d: 0x%02x' % (i, b), 'DB']])
            self.ss_databytebits = []

        elif bitnum > self.last_databit:
            return self.decode_frame_end(can_rx, bitnum)

        return False

    # Returns True if the frame ended (EOF), False otherwise.
    def decode_extended_frame(self, can_rx, bitnum):

        # Remember start of EID (see below).
        if bitnum == 14:
            self.ss_block = self.samplenum

        # Bits 14-31: Extended identifier (EID[17..0])
        elif bitnum == 31:
            self.eid = int(''.join(str(d) for d in self.bits[14:]), 2)
            s = '%d (0x%x)' % (self.eid, self.eid)
            self.putb([4, ['Extended Identifier: %s' % s,
                           'Extended ID: %s' % s, 'Extended ID', 'EID']])

            self.fullid = self.id << 18 | self.eid
            s = '%d (0x%x)' % (self.fullid, self.fullid)
            self.putb([5, ['Full Identifier: %s' % s, 'Full ID: %s' % s,
                           'Full ID', 'FID']])

            # Bit 12: Substitute remote request (SRR) bit
            self.put12([9, ['Substitute remote request: %d' % self.bits[12],
                            'SRR: %d' % self.bits[12], 'SRR']])

        # Bit 32: Remote transmission request (RTR) bit
        # Data frame: dominant, remote frame: recessive
        # Remote frames do not contain a data field.
        if bitnum == 32:
            rtr = 'remote' if can_rx == 1 else 'data'
            self.putx([8, ['Remote transmission request: %s frame' % rtr,
                           'RTR: %s frame' % rtr, 'RTR']])

        # Bit 33: RB1 (reserved bit)
        elif bitnum == 33:
            self.putx([7, ['Reserved bit 1: %d' % can_rx,
                           'RB1: %d' % can_rx, 'RB1']])

        # Bit 34: RB0 (reserved bit)
        elif bitnum == 34:
            self.putx([7, ['Reserved bit 0: %d' % can_rx,
                           'RB0: %d' % can_rx, 'RB0']])

        # Remember start of DLC (see below).
        elif bitnum == 35:
            self.ss_block = self.samplenum

        # Bits 35-38: Data length code (DLC), in number of bytes (0-8).
        elif bitnum == 38:
            self.dlc = int(''.join(str(d) for d in self.bits[35:38 + 1]), 2)
            self.putb([10, ['Data length code: %d' % self.dlc,
                            'DLC: %d' % self.dlc, 'DLC']])
            self.last_databit = 38 + (self.dlc * 8)

        # Remember all databyte bits, except the very last one.
        elif bitnum in range(39, self.last_databit):
            self.ss_databytebits.append(self.samplenum)

        # Bits 39-X: Data field (0-8 bytes, depending on DLC)
        # The bits within a data byte are transferred MSB-first.
        elif bitnum == self.last_databit:
            self.ss_databytebits.append(self.samplenum) # Last databyte bit.
            for i in range(self.dlc):
                x = 38 + (8 * i) + 1
                b = int(''.join(str(d) for d in self.bits[x:x + 8]), 2)
                ss = self.ss_databytebits[i * 8]
                es = self.ss_databytebits[((i + 1) * 8) - 1]
                self.putg(ss, es, [0, ['Data byte %d: 0x%02x' % (i, b),
                                       'DB %d: 0x%02x' % (i, b), 'DB']])
            self.ss_databytebits = []

        elif bitnum > self.last_databit:
            return self.decode_frame_end(can_rx, bitnum)

        return False

    def handle_bit(self, can_rx):
        self.rawbits.append(can_rx)
        self.bits.append(can_rx)

        # Get the index of the current CAN frame bit (without stuff bits).
        bitnum = len(self.bits) - 1

        # If this is a stuff bit, remove it from self.bits and ignore it.
        if self.is_stuff_bit():
            self.putx([15, [str(can_rx)]])
            self.curbit += 1 # Increase self.curbit (bitnum is not affected).
            return
        else:
            self.putx([17, [str(can_rx)]])

        # Bit 0: Start of frame (SOF) bit
        if bitnum == 0:
            if can_rx == 0:
                self.putx([1, ['Start of frame', 'SOF', 'S']])
            else:
                self.putx([16, ['Start of frame (SOF) must be a dominant bit']])

        # Remember start of ID (see below).
        elif bitnum == 1:
            self.ss_block = self.samplenum

        # Bits 1-11: Identifier (ID[10..0])
        # The bits ID[10..4] must NOT be all recessive.
        elif bitnum == 11:
            self.id = int(''.join(str(d) for d in self.bits[1:]), 2)
            s = '%d (0x%x)' % (self.id, self.id),
            self.putb([3, ['Identifier: %s' % s, 'ID: %s' % s, 'ID']])

        # RTR or SRR bit, depending on frame type (gets handled later).
        elif bitnum == 12:
            # self.putx([0, ['RTR/SRR: %d' % can_rx]]) # Debug only.
            self.ss_bit12 = self.samplenum

        # Bit 13: Identifier extension (IDE) bit
        # Standard frame: dominant, extended frame: recessive
        elif bitnum == 13:
            ide = self.frame_type = 'standard' if can_rx == 0 else 'extended'
            self.putx([6, ['Identifier extension bit: %s frame' % ide,
                           'IDE: %s frame' % ide, 'IDE']])

        # Bits 14-X: Frame-type dependent, passed to the resp. handlers.
        elif bitnum >= 14:
            if self.frame_type == 'standard':
                done = self.decode_standard_frame(can_rx, bitnum)
            else:
                done = self.decode_extended_frame(can_rx, bitnum)

            # The handlers return True if a frame ended (EOF).
            if done:
                return

        # After a frame there are 3 intermission bits (recessive).
        # After these bits, the bus is considered free.

        self.curbit += 1

    def decode(self, ss, es, data):
        if not self.samplerate:
            raise SamplerateError('Cannot decode without samplerate.')
        for (self.samplenum, pins) in data:

            (can_rx,) = pins
            data.itercnt += 1

            # State machine.
            if self.state == 'IDLE':
                # Wait for a dominant state (logic 0) on the bus.
                if can_rx == 1:
                    continue
                self.sof = self.samplenum
                self.state = 'GET BITS'
            elif self.state == 'GET BITS':
                # Wait until we're in the correct bit/sampling position.
                if not self.reached_bit(self.curbit):
                    continue
                self.handle_bit(can_rx)

