##
## This file is part of the libsigrokdecode project.
##
## Copyright (C) 2014 Guenther Wenninger <robin@bitschubbser.org>
## Copyright (C) 2022 DreamSourceLab <support@dreamsourcelab.com>
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

class SamplerateError(Exception):
    pass

class Decoder(srd.Decoder):
    api_version = 3
    id = 'spdif'
    name = 'S/PDIF'
    longname = 'Sony/Philips Digital Interface Format'
    desc = 'Serial bus for connecting digital audio devices.'
    license = 'gplv2+'
    inputs = ['logic']
    outputs = []
    tags = ['Audio', 'PC']
    channels = (
        {'id': 'data', 'name': 'Data', 'desc': 'Data line', 'idn':'dec_spdif_chan_data'},
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
        self.reset()

    def reset(self):
        self.state = 'GET FIRST PULSE WIDTH'
        self.ss_edge = None
        self.first_edge = True
        self.samplenum_prev_edge = 0
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
        
        self.temp_pulse_width = []
        self.temp_samplenum = []
        self.firstpramble = True


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
        self.temp_pulse_width.append(self.pulse_width)
        self.temp_samplenum.append(self.samplenum)
        if self.pulse_width != 0:
            self.clocks.append(self.pulse_width)
            self.state = 'GET SECOND PULSE WIDTH'
            
    def find_second_pulse_width(self):
        self.temp_pulse_width.append(self.pulse_width)
        self.temp_samplenum.append(self.samplenum)
        if self.pulse_width > (self.clocks[0] * 1.3) or \
                self.pulse_width < (self.clocks[0] * 0.7):
            self.clocks.append(self.pulse_width)
            self.state = 'GET THIRD PULSE WIDTH'

    def find_third_pulse_width(self):
        self.temp_pulse_width.append(self.pulse_width)
        self.temp_samplenum.append(self.samplenum)

        if ((self.pulse_width <= (self.clocks[0] * 1.3) and self.pulse_width >= (self.clocks[0] * 0.7)) or \
        (self.pulse_width <= (self.clocks[1] * 1.3) and self.pulse_width >= (self.clocks[1] * 0.7))):
            return

        self.clocks.append(self.pulse_width)
        self.clocks.sort()
        self.range1 = (self.clocks[0] + self.clocks[1]) / 2
        self.range2 = (self.clocks[1] + self.clocks[2]) / 2

        spdif_bitrate = int(self.samplerate / (self.clocks[2] / 1.5))
        self.ss_edge = 0

        self.putx(0,self.temp_samplenum[0]-24,[0, ['Signal Bitrate: %d Mbit/s (=> %d kHz)' % \
                  (spdif_bitrate, (spdif_bitrate/ (2 * 32)))]])
        clock_period_nsec = 1000000000 / spdif_bitrate

        self.last_preamble = self.samplenum

        is_preamble_status = True
        while len(self.decode_re_get_pulse_type())>0:
             if is_preamble_status == True:
                if self.decode_recheck_preablm() == 0:
                    return
                else:
                    is_preamble_status = False
             else:
                if self.decode_recheck_stream() == 0:
                    return
                else:
                    is_preamble_status = True
        self.state = 'DECODE STREAM'


    def decode_re_get_pulse_type(self):
        pulse_type =[]
        for temp_pulse in self.temp_pulse_width:
            if self.range1 == 0 or self.range2 == 0:
                 return pulse_type
            if temp_pulse >= self.range2:
                 pulse_type.append(2)
            elif temp_pulse >= self.range1:
                 pulse_type.append(0)
            else :
                 pulse_type.append(1)
        return pulse_type

    def decode_recheck_preablm(self):
        if self.decode_re_get_pulse_type() == -1:
            return -1

        pulse_type = self.decode_re_get_pulse_type()
        temp_preamble = []
        preamble_is_ok = 0
        preamble_state = -1

        while len(pulse_type)>0:
            temp_samnum = self.temp_samplenum.pop(0)
            temp_pul_type = pulse_type.pop(0)
            temp_pul_width = self.temp_pulse_width.pop(0)
            if preamble_state == -1 and temp_pul_type == 2:
                temp_preamble.append(temp_pul_type)
                preamble_state = 0
                self.ss_edge = temp_samnum - temp_pul_width - 1
            elif preamble_state == 0:
                temp_preamble.append(temp_pul_type)
                preamble_state = 1
            elif preamble_state == 1:
                temp_preamble.append(temp_pul_type)
                preamble_state = 2
            elif preamble_state == 2:
                temp_preamble.append(temp_pul_type)
                if temp_preamble == [2, 0, 1, 0]:
                    self.putx(self.ss_edge,temp_samnum,[1, ['Preamble W', 'W']])
                    self.seen_preamble = True
                elif temp_preamble == [2, 2, 1, 1]:
                    self.putx(self.ss_edge,temp_samnum,[1, ['Preamble M', 'M']])
                    self.seen_preamble = True
                elif temp_preamble == [2, 1, 1, 2]:
                    self.putx(self.ss_edge,temp_samnum,[1, ['Preamble B', 'B']])
                    self.seen_preamble = True
                else:
                    self.putx(self.ss_edge,temp_samnum,[1, ['Unknown Preamble', 'Unknown Prea.', 'U']])
                preamble_state = -1
                preamble_is_ok = 1
                temp_preamble = []
     
                self.bitcount = 0
                self.first_one = True
                self.last_preamble = temp_samnum
                break

        if preamble_is_ok == 1:
            if len(pulse_type) == 0:
                self.state = 'DECODE STREAM'
                return 0
            else:
                return 1
        else:
            self.state = 'DECODE PREAMBLE'
            self.preamble = temp_preamble.copy()
            if preamble_state == -1:
                self.preamble_state = 0
            else:
                self.preamble_state = preamble_state
            return 0
    
    def decode_recheck_stream(self):
        if self.decode_re_get_pulse_type() == -1:
            return -1

        pulse_type = self.decode_re_get_pulse_type()
        subframe = []
        first_one = True
        subframe_is_ok = 0
        bitcount = 0

        while len(pulse_type)>0:
            samnum = self.temp_samplenum.pop(0)
            pul_type = pulse_type.pop(0)
            pul_width = self.temp_pulse_width.pop(0)
            if pul_type == 1 and first_one:
               first_one = False
               subframe.append([pul_type,samnum - \
                    pul_width -1,samnum])
            elif pul_type == 1 and not first_one:
                subframe[-1][2] = samnum
                self.putx(subframe[-1][1],samnum, [2, ['1']])
                bitcount += 1
                first_one = True
            else:
                subframe.append([pul_type,samnum - \
                    pul_width -1,samnum])
                self.putx(samnum - pul_width - 1,
                    samnum,[2,['0']])
                bitcount += 1
            
            if bitcount == 28:
                aux_audio_data = subframe[0:4]
                sam, sam_rot = '', ''
                for a in aux_audio_data:
                    sam = sam + str(a[0])
                    sam_rot = str(a[0]) + sam_rot
                sample = subframe[4:24]
                for s in sample:
                    sam = sam + str(s[0])
                    sam_rot = str(s[0]) + sam_rot
                validity = subframe[24:25]
                subcode_data = subframe[25:26]
                channel_status = subframe[26:27]
                parity = subframe[27:28]

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

                subframe = []
                self.seen_preamble = False
                bitcount = 0
                subframe_is_ok = 1
                break
        
        if subframe_is_ok == 1:
            if len(pulse_type) == 0:
                self.state = 'DECODE STREAM'
                return 0
            else:
                return 1
        else:
            self.state = 'DECODE STREAM'
            self.subframe = subframe.copy()
            self.bitcount = bitcount
            self.first_one =first_one
            return 0


    def decode_stream(self):
        pulse = self.get_pulse_type()
        
        if not self.seen_preamble:
            # This is probably the start of a preamble, decode it.
            if pulse == 2:
                self.preamble.append(self.get_pulse_type())
                self.preamble_state == 0
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

    #Unknown Preamble
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
                self.seen_preamble = True
            elif self.preamble == [2, 2, 1, 1]:
                self.puty([1, ['Preamble M', 'M']])
                self.seen_preamble = True
            elif self.preamble == [2, 1, 1, 2]:
                self.puty([1, ['Preamble B', 'B']])
                self.seen_preamble = True
            else:
                self.puty([1, ['Unknown Preamble', 'Unknown Prea.', 'U']])
            self.preamble = []
            self.bitcount = 0
            self.first_one = True

        self.last_preamble = self.samplenum

    def decode(self):
        if not self.samplerate:
            raise SamplerateError('Cannot decode without samplerate.')

        # Throw away first detected edge as it might be mangled data.
        self.wait({0: 'e'})
        self.samplenum_prev_edge = self.samplenum

        while True:
            # Wait for any edge (rising or falling).
            (data,) = self.wait({0: 'e'})
            self.pulse_width = self.samplenum - self.samplenum_prev_edge - 1
            self.samplenum_prev_edge = self.samplenum

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
