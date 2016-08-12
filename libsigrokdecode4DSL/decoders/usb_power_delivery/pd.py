##
## This file is part of the libsigrokdecode project.
##
## Copyright (C) 2015 Google, Inc
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
import struct
import zlib   # for crc32

# BMC encoding with a 600kHz datarate
UI_US = 1000000/600000.0

# Threshold to discriminate half-1 from 0 in Binary Mark Conding
THRESHOLD_US = (UI_US + 2 * UI_US) / 2

# Control Message type
CTRL_TYPES = {
    0: 'reserved',
    1: 'GOOD CRC',
    2: 'GOTO MIN',
    3: 'ACCEPT',
    4: 'REJECT',
    5: 'PING',
    6: 'PS RDY',
    7: 'GET SOURCE CAP',
    8: 'GET SINK CAP',
    9: 'DR SWAP',
    10: 'PR SWAP',
    11: 'VCONN SWAP',
    12: 'WAIT',
    13: 'SOFT RESET',
    14: 'reserved',
    15: 'reserved'
}

# Data message type
DATA_TYPES = {
    1: 'SOURCE CAP',
    2: 'REQUEST',
    3: 'BIST',
    4: 'SINK CAP',
    15: 'VDM'
}

# 4b5b encoding of the symbols
DEC4B5B = [
    0x10,   # Error      00000
    0x10,   # Error      00001
    0x10,   # Error      00010
    0x10,   # Error      00011
    0x10,   # Error      00100
    0x10,   # Error      00101
    0x13,   # Sync-3     00110
    0x14,   # RST-1      00111
    0x10,   # Error      01000
    0x01,   # 1 = 0001   01001
    0x04,   # 4 = 0100   01010
    0x05,   # 5 = 0101   01011
    0x10,   # Error      01100
    0x16,   # EOP        01101
    0x06,   # 6 = 0110   01110
    0x07,   # 7 = 0111   01111
    0x10,   # Error      10000
    0x12,   # Sync-2     10001
    0x08,   # 8 = 1000   10010
    0x09,   # 9 = 1001   10011
    0x02,   # 2 = 0010   10100
    0x03,   # 3 = 0011   10101
    0x0A,   # A = 1010   10110
    0x0B,   # B = 1011   10111
    0x11,   # Sync-1     11000
    0x15,   # RST-2      11001
    0x0C,   # C = 1100   11010
    0x0D,   # D = 1101   11011
    0x0E,   # E = 1110   11100
    0x0F,   # F = 1111   11101
    0x00,   # 0 = 0000   11110
    0x10,   # Error      11111
]
SYM_ERR = 0x10
SYNC1 = 0x11
SYNC2 = 0x12
SYNC3 = 0x13
RST1 = 0x14
RST2 = 0x15
EOP = 0x16
SYNC_CODES = [SYNC1, SYNC2, SYNC3]
HRST_CODES = [RST1, RST1, RST1, RST2]

START_OF_PACKETS = {
    (SYNC1, SYNC1, SYNC1, SYNC2): 'SOP',
    (SYNC1, SYNC1, SYNC3, SYNC3): "SOP'",
    (SYNC1, SYNC3, SYNC1, SYNC3): 'SOP"',
    (SYNC1, RST2,  RST2,  SYNC3): "SOP' Debug",
    (SYNC1, RST2,  SYNC3, SYNC2): 'SOP" Debug',
    (RST1,  SYNC1, RST1,  SYNC3): 'Cable Reset',
    (RST1,  RST1,  RST1,   RST2): 'Hard Reset',
}

SYM_NAME = [
    ['0x0', '0'],
    ['0x1', '1'],
    ['0x2', '2'],
    ['0x3', '3'],
    ['0x4', '4'],
    ['0x5', '5'],
    ['0x6', '6'],
    ['0x7', '7'],
    ['0x8', '8'],
    ['0x9', '9'],
    ['0xA', 'A'],
    ['0xB', 'B'],
    ['0xC', 'C'],
    ['0xD', 'D'],
    ['0xE', 'E'],
    ['0xF', 'F'],
    ['ERROR', 'X'],
    ['SYNC-1', 'S1'],
    ['SYNC-2', 'S2'],
    ['SYNC-3', 'S3'],
    ['RST-1', 'R1'],
    ['RST-2', 'R2'],
    ['EOP', '#'],
]

RDO_FLAGS = {
    (1 << 24): 'no_suspend',
    (1 << 25): 'comm_cap',
    (1 << 26): 'cap_mismatch',
    (1 << 27): 'give_back'
}
PDO_TYPE = ['', 'BATT:', 'VAR:', '<bad>']
PDO_FLAGS = {
    (1 << 29): 'dual_role_power',
    (1 << 28): 'suspend',
    (1 << 27): 'ext',
    (1 << 26): 'comm_cap',
    (1 << 25): 'dual_role_data'
}

BIST_MODES = {
        0: 'Receiver',
        1: 'Transmit',
        2: 'Counters',
        3: 'Carrier 0',
        4: 'Carrier 1',
        5: 'Carrier 2',
        6: 'Carrier 3',
        7: 'Eye',
}

VDM_CMDS = {
        1: 'Disc Ident',
        2: 'Disc SVID',
        3: 'Disc Mode',
        4: 'Enter Mode',
        5: 'Exit Mode',
        6: 'Attention',
        # 16..31: SVID Specific Commands
        # DisplayPort Commands
        16: 'DP Status',
        17: 'DP Configure',
}
VDM_ACK = ['REQ', 'ACK', 'NAK', 'BSY']

class SamplerateError(Exception):
    pass

class Decoder(srd.Decoder):
    api_version = 2
    id = 'usb_power_delivery'
    name = 'USB PD'
    longname = 'USB Power Delivery'
    desc = 'USB Power Delivery protocol.'
    license = 'gplv2+'
    inputs = ['logic']
    outputs = ['usb_pd']
    channels = (
        {'id': 'cc', 'name': 'CC', 'desc': 'Control channel'},
    )
    options = (
        {'id': 'fulltext', 'desc': 'full text decoding of the packet',
         'default': 'no', 'values': ('yes', 'no')},
    )
    annotations = (
        ('type', 'Packet Type'),
        ('Preamble', 'Preamble'),
        ('SOP', 'Start of Packet'),
        ('Head', 'Header'),
        ('Data', 'Data'),
        ('CRC', 'Checksum'),
        ('EOP', 'End Of Packet'),
        ('Sym', '4b5b symbols'),
        ('warnings', 'Warnings'),
        ('src', 'Source Message'),
        ('snk', 'Sink Message'),
        ('payload', 'Payload'),
        ('text', 'Plain text'),
    )
    annotation_rows = (
       ('4B5B', 'symbols', (7, )),
       ('Phase', 'parts', (1, 2, 3, 4, 5, 6, )),
       ('payload', 'Payload', (11, )),
       ('type', 'Type', (0, 9, 10, )),
       ('warnings', 'Warnings', (8, )),
       ('text', 'Full text', (12, )),
    )
    binary = (
        ('raw-data', 'RAW binary data'),
    )

    def get_request(self, rdo):
        pos = (rdo >> 28) & 7
        op_ma = ((rdo >> 10) & 0x3ff) * 10
        max_ma = (rdo & 0x3ff) * 10
        flags = ''
        for f in RDO_FLAGS.keys():
            if rdo & f:
                flags += ' ' + RDO_FLAGS[f]
        return '[%d]%d/%d mA%s' % (pos, op_ma, max_ma, flags)

    def get_source_cap(self, pdo):
        t = (pdo >> 30) & 3
        if t == 0:
            mv = ((pdo >> 10) & 0x3ff) * 50
            ma = ((pdo >> 0) & 0x3ff) * 10
            p = '%.1fV %.1fA' % (mv/1000.0, ma/1000.0)
        elif t == 1:
            minv = ((pdo >> 10) & 0x3ff) * 50
            maxv = ((pdo >> 20) & 0x3ff) * 50
            mw = ((pdo >> 0) & 0x3ff) * 250
            p = '%.1f/%.1fV %.1fW' % (minv/1000.0, maxv/1000.0, mw/1000.0)
        elif t == 2:
            minv = ((pdo >> 10) & 0x3ff) * 50
            maxv = ((pdo >> 20) & 0x3ff) * 50
            ma = ((pdo >> 0) & 0x3ff) * 10
            p = '%.1f/%.1fV %.1fA' % (minv/1000.0, maxv/1000.0, ma/1000.0)
        else:
            p = ''
        flags = ''
        for f in PDO_FLAGS.keys():
            if pdo & f:
                flags += ' ' + PDO_FLAGS[f]
        return '%s%s%s' % (PDO_TYPE[t], p, flags)

    def get_sink_cap(self, pdo):
        t = (pdo >> 30) & 3
        if t == 0:
            mv = ((pdo >> 10) & 0x3ff) * 50
            ma = ((pdo >> 0) & 0x3ff) * 10
            p = '%.1fV %.1fA' % (mv/1000.0, ma/1000.0)
        elif t == 1:
            minv = ((pdo >> 10) & 0x3ff) * 50
            maxv = ((pdo >> 20) & 0x3ff) * 50
            mw = ((pdo >> 0) & 0x3ff) * 250
            p = '%.1f/%.1fV %.1fW' % (minv/1000.0, maxv/1000.0, mw/1000.0)
        elif t == 2:
            minv = ((pdo >> 10) & 0x3ff) * 50
            maxv = ((pdo >> 20) & 0x3ff) * 50
            ma = ((pdo >> 0) & 0x3ff) * 10
            p = '%.1f/%.1fV %.1fA' % (minv/1000.0, maxv/1000.0, ma/1000.0)
        else:
            p = ''
        flags = ''
        for f in PDO_FLAGS.keys():
            if pdo & f:
                flags += ' ' + PDO_FLAGS[f]
        return '%s%s%s' % (PDO_TYPE[t], p, flags)

    def get_vdm(self, idx, data):
        if idx == 0:    # VDM header
            vid = data >> 16
            struct = data & (1 << 15)
            txt = 'VDM'
            if struct:  # Structured VDM
                cmd = data & 0x1f
                src = data & (1 << 5)
                ack = (data >> 6) & 3
                pos = (data >> 8) & 7
                ver = (data >> 13) & 3
                txt = VDM_ACK[ack] + ' '
                txt += VDM_CMDS[cmd] if cmd in VDM_CMDS else 'cmd?'
                txt += ' pos %d' % (pos) if pos else ' '
            else:   # Unstructured VDM
                txt = 'unstruct [%04x]' % (data & 0x7fff)
            txt += ' SVID:%04x' % (vid)
        else:   # VDM payload
            txt = 'VDO:%08x' % (data)
        return txt

    def get_bist(self, idx, data):
        mode = data >> 28
        counter = data & 0xffff
        mode_name = BIST_MODES[mode] if mode in BIST_MODES else 'INVALID'
        if mode == 2:
            mode_name = 'Counter[= %d]' % (counter)
        # TODO check all 0 bits are 0 / emit warnings
        return 'mode %s' % (mode_name) if idx == 0 else 'invalid BRO'

    def putpayload(self, s0, s1, idx):
        t = self.head_type()
        txt = '???'
        if t == 2:
            txt = self.get_request(self.data[idx])
        elif t == 1:
            txt = self.get_source_cap(self.data[idx])
        elif t == 4:
            txt = self.get_sink_cap(self.data[idx])
        elif t == 15:
            txt = self.get_vdm(idx, self.data[idx])
        elif t == 3:
            txt = self.get_bist(idx, self.data[idx])
        self.putx(s0, s1, [11, [txt, txt]])
        self.text += ' - ' + txt

    def puthead(self):
        ann_type = 9 if self.head_power_role() else 10
        role = 'SRC' if self.head_power_role() else 'SNK'
        if self.head_data_role() != self.head_power_role():
            role += '/DFP' if self.head_data_role() else '/UFP'
        t = self.head_type()
        if self.head_count() == 0:
            shortm = CTRL_TYPES[t]
        else:
            shortm = DATA_TYPES[t] if t in DATA_TYPES else 'DAT???'

        longm = '{:s}[{:d}]:{:s}'.format(role, self.head_id(), shortm)
        self.putx(0, -1, [ann_type, [longm, shortm]])
        self.text += longm

    def head_id(self):
        return (self.head >> 9) & 7

    def head_power_role(self):
        return (self.head >> 8) & 1

    def head_data_role(self):
        return (self.head >> 5) & 1

    def head_rev(self):
        return ((self.head >> 6) & 3) + 1

    def head_type(self):
        return self.head & 0xF

    def head_count(self):
        return (self.head >> 12) & 7

    def putx(self, s0, s1, data):
        self.put(self.edges[s0], self.edges[s1], self.out_ann, data)

    def putwarn(self, longm, shortm):
        self.putx(0, -1, [8, [longm, shortm]])

    def compute_crc32(self):
        bdata = struct.pack('<H'+'I'*len(self.data), self.head & 0xffff,
                            *tuple([d & 0xffffffff for d in self.data]))
        return zlib.crc32(bdata)

    def rec_sym(self, i, sym):
        self.putx(i, i+5, [7, SYM_NAME[sym]])

    def get_sym(self, i, rec=True):
        v = (self.bits[i] | (self.bits[i+1] << 1) | (self.bits[i+2] << 2) |
             (self.bits[i+3] << 3) | (self.bits[i+4] << 4))
        sym = DEC4B5B[v]
        if rec:
            self.rec_sym(i, sym)
        return sym

    def get_short(self):
        i = self.idx
        # Check it's not a truncated packet
        if len(self.bits) - i <= 20:
            self.putwarn('Truncated', '!')
            return 0x0BAD
        k = [self.get_sym(i), self.get_sym(i+5),
             self.get_sym(i+10), self.get_sym(i+15)]
        # TODO check bad symbols
        val = k[0] | (k[1] << 4) | (k[2] << 8) | (k[3] << 12)
        self.idx += 20
        return val

    def get_word(self):
        lo = self.get_short()
        hi = self.get_short()
        return lo | (hi << 16)

    def find_corrupted_sop(self, k):
        # Start of packet are valid even if they have only 3 correct symbols
        # out of 4.
        for seq in START_OF_PACKETS.keys():
            if [k[i] == seq[i] for i in range(len(k))].count(True) >= 3:
                return START_OF_PACKETS[seq]
        return None

    def scan_eop(self):
        for i in range(len(self.bits) - 19):
            k = (self.get_sym(i, rec=False), self.get_sym(i+5, rec=False),
                 self.get_sym(i+10, rec=False), self.get_sym(i+15, rec=False))
            sym = START_OF_PACKETS[k] if k in START_OF_PACKETS else None
            if not sym:
                sym = self.find_corrupted_sop(k)
            # We have an interesting symbol sequence
            if sym:
                # annotate the preamble
                self.putx(0, i, [1, ['Preamble', '...']])
                # annotate each symbol
                self.rec_sym(i, k[0])
                self.rec_sym(i+5, k[1])
                self.rec_sym(i+10, k[2])
                self.rec_sym(i+15, k[3])
                if sym == 'Hard Reset':
                    self.text += 'HRST'
                    return -1   # Hard reset
                elif sym == 'Cable Reset':
                    self.text += 'CRST'
                    return -1   # Cable reset
                else:
                    self.putx(i, i+20, [2, [sym, 'S']])
                return i+20
        self.putx(0, len(self.bits), [1, ['Junk???', 'XXX']])
        self.text += 'Junk???'
        self.putwarn('No start of packet found', 'XXX')
        return -1   # No Start Of Packet

    def __init__(self):
        self.samplerate = None
        self.idx = 0
        self.packet_seq = 0
        self.samplenum = 0
        self.previous = 0
        self.oldpins = [0]
        self.startsample = None
        self.bits = []
        self.edges = []
        self.bad = []
        self.half_one = False
        self.start_one = 0

    def metadata(self, key, value):
        if key == srd.SRD_CONF_SAMPLERATE:
            self.samplerate = value
            # 0 is 2 UI, space larger than 1.5x 0 is definitely wrong
            self.maxbit = self.us2samples(3 * UI_US)
            # duration threshold between half 1 and 0
            self.threshold = self.us2samples(THRESHOLD_US)

    def start(self):
        self.out_python = self.register(srd.OUTPUT_PYTHON)
        self.out_ann = self.register(srd.OUTPUT_ANN)
        self.out_binary = self.register(srd.OUTPUT_BINARY)
        self.out_bitrate = self.register(
            srd.OUTPUT_META,
            meta=(int, 'Bitrate', 'Bitrate during the packet')
        )

    def us2samples(self, us):
        if not self.samplerate:
            raise SamplerateError('Need the samplerate.')
        return int(us * self.samplerate / 1000000)

    def decode_packet(self):
        self.data = []
        self.idx = 0
        self.text = ''

        if len(self.edges) < 50:
            return  # Not a real PD packet

        self.packet_seq += 1
        tstamp = float(self.startsample) / self.samplerate
        self.text += '#%-4d (%8.6fms): ' % (self.packet_seq, tstamp*1000)

        self.idx = self.scan_eop()
        if self.idx < 0:
            # Full text trace of the issue
            self.putx(0, self.idx, [12, [self.text, '...']])
            return  # No real packet: ABORT

        # Packet header
        self.head = self.get_short()
        self.putx(self.idx-20, self.idx, [3, ['H:%04x' % (self.head), 'HD']])
        self.puthead()

        # Decode data payload
        for i in range(self.head_count()):
            self.data.append(self.get_word())
            self.putx(self.idx-40, self.idx,
                      [4, ['[%d]%08x' % (i, self.data[i]), 'D%d' % (i)]])
            self.putpayload(self.idx-40, self.idx, i)

        # CRC check
        self.crc = self.get_word()
        ccrc = self.compute_crc32()
        if self.crc != ccrc:
            self.putwarn('Bad CRC %08x != %08x' % (self.crc, ccrc), 'CRC!')
        self.putx(self.idx-40, self.idx, [5, ['CRC:%08x' % (self.crc), 'CRC']])

        # End of Packet
        if len(self.bits) >= self.idx + 5 and self.get_sym(self.idx) == EOP:
            self.putx(self.idx, self.idx + 5, [6, ['EOP', 'E']])
            self.idx += 5
        else:
            self.putwarn('No EOP', 'EOP!')
        # Full text trace
        if self.options['fulltext'] == 'yes':
            self.putx(0, self.idx, [12, [self.text, '...']])

        # Meta data for bitrate
        ss, es = self.edges[0], self.edges[-1]
        bitrate = self.samplerate*len(self.bits) / float(es - ss)
        self.put(es, ss, self.out_bitrate, int(bitrate))
        # Raw binary data (BMC decoded)
        #self.put(es, ss, self.out_binary, [0, bytes(self.bits)])

    def decode(self, ss, es, data):
        if not self.samplerate:
            raise SamplerateError('Cannot decode without samplerate.')
        for (self.samplenum, pins) in data:
            data.itercnt += 1
            # find edges ...
            if self.oldpins == pins:
                continue

            self.oldpins, (cc, ) = pins, pins

            # First sample of the packet, just record the start date
            if not self.startsample:
                self.startsample = self.samplenum
                self.previous = self.samplenum
                continue

            diff = self.samplenum - self.previous

            # Large idle: use it as the end of packet
            if diff > self.maxbit:
                # the last edge of the packet
                self.edges.append(self.previous)
                # Export the packet
                self.decode_packet()
                # Reset for next packet
                self.startsample = self.samplenum
                self.bits = []
                self.edges = []
                self.bad = []
                self.half_one = False
                self.start_one = 0
            else:   # add the bit to the packet
                is_zero = diff > self.threshold
                if is_zero and not self.half_one:
                    self.bits.append(0)
                    self.edges.append(self.previous)
                elif not is_zero and self.half_one:
                    self.bits.append(1)
                    self.edges.append(self.start_one)
                    self.half_one = False
                elif not is_zero and not self.half_one:
                    self.half_one = True
                    self.start_one = self.previous
                else:   # Invalid BMC sequence
                    self.bad.append((self.start_one, self.previous))
                    # TODO try to recover
                    self.bits.append(0)
                    self.edges.append(self.previous)
                    self.half_one = False
            self.previous = self.samplenum
