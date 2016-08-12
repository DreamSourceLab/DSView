##
## This file is part of the libsigrokdecode project.
##
## Copyright (C) 2014 Guenther Wenninger <robin@bitschubbser.org>
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
    id = 'spdif'
    name = 'S/PDIF'
    longname = 'Sony/Philips Digital Interface Format'
    desc = 'Serial bus for connecting digital audio devices.'
    license = 'gplv2+'
    inputs = ['logic']
    outputs = ['spdif']
    channels = (
        {'id': 'data', 'name': 'Data', 'desc': 'Data line'},
    )
    annotations = (
        ('bitrate', 'Bitrate / baudrate'),
        ('preamble', 'Preamble'),
        ('bits', 'Bits'),
        ('aux', 'Auxillary-audio-databits'),
        ('samples', 'Audio Samples'),
        ('validity', 'Data Valid'),
        ('subcode', 'Subcode data'),
        ('chan_stat', 'Channnel Status'),
        ('parity', 'Parity Bit'),
    )
    annotation_rows = (
        ('info', 'Info', (0, 1, 3, 5, 6, 7, 8)),
        ('bits', 'Bits', (2,)),
        ('samples', 'Samples', (4,)),
    )

    def putx(self, ss, es, data):
        self.put(ss, es, self.out_ann, data)

    def puty(self, data):
        self.put(self.ss_edge, self.samplenum, self.out_ann, data)

    def __init__(self):
        self.state = 'GET FIRST PULSE WIDTH'
        self.olddata = None
        self.ss_edge = None
        self.first_edge = True
        self.pulse_width = 0

        self.clocks = []
        self.range1 = 0
        self.range2 = 0

        self.preamble_state = 0
        self.preamble = []
        self.seen_preamble = False
        self.last_preamble = 0

        self.first_one = True
        self.subframe = []

    def start(self):
        self.out_ann = self.register(srd.OUTPUT_ANN)

    def metadata(self, key, value):
        if key == srd.SRD_CONF_SAMPLERATE:
            self.samplerate = value

    def get_pulse_type(self):
        if self.range1 == 0 or self.range2 == 0:
            return -1
        if self.pulse_width >= self.range2:
            return 2
        elif self.pulse_width >= self.range1:
            return 0
        else:
            return 1

    def find_first_pulse_width(self):
        if self.pulse_width != 0:
            self.clocks.append(self.pulse_width)
            self.state = 'GET SECOND PULSE WIDTH'

    def find_second_pulse_width(self):
        if self.pulse_width > (self.clocks[0] * 1.3) or \
                self.pulse_width < (self.clocks[0] * 0.7):
            self.clocks.append(self.pulse_width)
            self.state = 'GET THIRD PULSE WIDTH'

    def find_third_pulse_width(self):
        if not ((self.pulse_width > (self.clocks[0] * 1.3) or \
                self.pulse_width < (self.clocks[0] * 0.7)) \
                and (self.pulse_width > (self.clocks[1] * 1.3) or \
                self.pulse_width < (self.clocks[1] * 0.7))):
            return

        self.clocks.append(self.pulse_width)
        self.clocks.sort()
        self.range1 = (self.clocks[0] + self.clocks[1]) / 2
        self.range2 = (self.clocks[1] + self.clocks[2]) / 2
        spdif_bitrate = int(self.samplerate / (self.clocks[2] / 1.5))
        self.ss_edge = 0

        self.puty([0, ['Signal Bitrate: %d Mbit/s (=> %d kHz)' % \
                  (spdif_bitrate, (spdif_bitrate/ (2 * 32)))]])

        clock_period_nsec = 1000000000 / spdif_bitrate

        self.last_preamble = self.samplenum

        # We are done recovering the clock, now let's decode the data stream.
        self.state = 'DECODE STREAM'

    def decode_stream(self):
        pulse = self.get_pulse_type()

        if not self.seen_preamble:
            # This is probably the start of a preamble, decode it.
            if pulse == 2:
                self.preamble.append(self.get_pulse_type())
                self.state = 'DECODE PREAMBLE'
                self.ss_edge = self.samplenum - self.pulse_width - 1
            return

        # We've seen a preamble.
        if pulse == 1 and self.first_one:
            self.first_one = False
            self.subframe.append([pulse, self.samplenum - \
                self.pulse_width - 1, self.samplenum])
        elif pulse == 1 and not self.first_one:
            self.subframe[-1][2] = self.samplenum
            self.putx(self.subframe[-1][1], self.samplenum, [2, ['1']])
            self.bitcount += 1
            self.first_one = True
        else:
            self.subframe.append([pulse, self.samplenum - \
                self.pulse_width - 1, self.samplenum])
            self.putx(self.samplenum - self.pulse_width - 1,
                      self.samplenum, [2, ['0']])
            self.bitcount += 1

        if self.bitcount == 28:
            aux_audio_data = self.subframe[0:4]
            sam, sam_rot = '', ''
            for a in aux_audio_data:
                sam = sam + str(a[0])
                sam_rot = str(a[0]) + sam_rot
            sample = self.subframe[4:24]
            for s in sample:
                sam = sam + str(s[0])
                sam_rot = str(s[0]) + sam_rot
            validity = self.subframe[24:25]
            subcode_data = self.subframe[25:26]
            channel_status = self.subframe[26:27]
            parity = self.subframe[27:28]

            self.putx(aux_audio_data[0][1], aux_audio_data[3][2], \
                      [3, ['Aux 0x%x' % int(sam, 2), '0x%x' % int(sam, 2)]])
            self.putx(sample[0][1], sample[19][2], \
                      [3, ['Sample 0x%x' % int(sam, 2), '0x%x' % int(sam, 2)]])
            self.putx(aux_audio_data[0][1], sample[19][2], \
                      [4, ['Audio 0x%x' % int(sam_rot, 2), '0x%x' % int(sam_rot, 2)]])
            if validity[0][0] == 0:
                self.putx(validity[0][1], validity[0][2], [5, ['V']])
            else:
                self.putx(validity[0][1], validity[0][2], [5, ['E']])
            self.putx(subcode_data[0][1], subcode_data[0][2],
                [6, ['S: %d' % subcode_data[0][0]]])
            self.putx(channel_status[0][1], channel_status[0][2],
                [7, ['C: %d' % channel_status[0][0]]])
            self.putx(parity[0][1], parity[0][2], [8, ['P: %d' % parity[0][0]]])

            self.subframe = []
            self.seen_preamble = False
            self.bitcount = 0

    def decode_preamble(self):
        if self.preamble_state == 0:
            self.preamble.append(self.get_pulse_type())
            self.preamble_state = 1
        elif self.preamble_state == 1:
            self.preamble.append(self.get_pulse_type())
            self.preamble_state = 2
        elif self.preamble_state == 2:
            self.preamble.append(self.get_pulse_type())
            self.preamble_state = 0
            self.state = 'DECODE STREAM'
            if self.preamble == [2, 0, 1, 0]:
                self.puty([1, ['Preamble W', 'W']])
            elif self.preamble == [2, 2, 1, 1]:
                self.puty([1, ['Preamble M', 'M']])
            elif self.preamble == [2, 1, 1, 2]:
                self.puty([1, ['Preamble B', 'B']])
            else:
                self.puty([1, ['Unknown Preamble', 'Unknown Prea.', 'U']])
            self.preamble = []
            self.seen_preamble = True
            self.bitcount = 0
            self.first_one = True

        self.last_preamble = self.samplenum

    def decode(self, ss, es, logic):
        if not self.samplerate:
            raise SamplerateError('Cannot decode without samplerate.')

        for (self.samplenum, pins) in logic:
            data = pins[0]
            logic.itercnt += 1

            # Initialize self.olddata with the first sample value.
            if self.olddata is None:
                self.olddata = data
                continue

            # First we need to recover the clock.
            if self.olddata == data:
                self.pulse_width += 1
                continue

            # Found rising or falling edge.
            if self.first_edge:
                # Throw away first detected edge as it might be mangled data.
                self.first_edge = False
                self.pulse_width = 0
            else:
                if self.state == 'GET FIRST PULSE WIDTH':
                    self.find_first_pulse_width()
                elif self.state == 'GET SECOND PULSE WIDTH':
                    self.find_second_pulse_width()
                elif self.state == 'GET THIRD PULSE WIDTH':
                    self.find_third_pulse_width()
                elif self.state == 'DECODE STREAM':
                    self.decode_stream()
                elif self.state == 'DECODE PREAMBLE':
                    self.decode_preamble()

            self.pulse_width = 0

            self.olddata = data
