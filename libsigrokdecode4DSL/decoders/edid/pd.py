##
## This file is part of the libsigrokdecode project.
##
## Copyright (C) 2012 Bert Vermeulen <bert@biot.com>
##
## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 3 of the License, or
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

# TODO:
#    - EDID < 1.3
#    - add short annotations
#    - Signal level standard field in basic display parameters block
#    - Additional color point descriptors
#    - Additional standard timing descriptors
#    - Extensions

import sigrokdecode as srd
import os

EDID_HEADER = [0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00]
OFF_VENDOR = 8
OFF_VERSION = 18
OFF_BASIC = 20
OFF_CHROM = 25
OFF_EST_TIMING = 35
OFF_STD_TIMING = 38
OFF_DET_TIMING = 54
OFF_NUM_EXT = 126
OFF_CHECKSUM = 127

# Pre-EDID established timing modes
est_modes = [
    '720x400@70Hz',
    '720x400@88Hz',
    '640x480@60Hz',
    '640x480@67Hz',
    '640x480@72Hz',
    '640x480@75Hz',
    '800x600@56Hz',
    '800x600@60Hz',
    '800x600@72Hz',
    '800x600@75Hz',
    '832x624@75Hz',
    '1024x768@87Hz(i)',
    '1024x768@60Hz',
    '1024x768@70Hz',
    '1024x768@75Hz',
    '1280x1024@75Hz',
    '1152x870@75Hz',
]

# X:Y display aspect ratios, as used in standard timing modes
xy_ratio = [
    (16, 10),
    (4, 3),
    (5, 4),
    (16, 9),
]

# Annotation classes
ANN_FIELDS = 0
ANN_SECTIONS = 1

class Decoder(srd.Decoder):
    api_version = 2
    id = 'edid'
    name = 'EDID'
    longname = 'Extended Display Identification Data'
    desc = 'Data structure describing display device capabilities.'
    license = 'gplv3+'
    inputs = ['i2c']
    outputs = ['edid']
    annotations = (
        ('fields', 'EDID structure fields'),
        ('sections', 'EDID structure sections'),
    )
    annotation_rows = (
        ('sections', 'Sections', (1,)),
        ('fields', 'Fields', (0,)),
    )

    def __init__(self):
        self.state = None
        # Received data items, used as an index into samplenum/data
        self.cnt = 0
        # Start/end sample numbers per data item
        self.sn = []
        # Received data
        self.cache = []

    def start(self):
        self.out_ann = self.register(srd.OUTPUT_ANN)

    def decode(self, ss, es, data):
        cmd, data = data

        # We only care about actual data bytes that are read (for now).
        if cmd != 'DATA READ':
            return

        self.cnt += 1
        self.sn.append([ss, es])
        self.cache.append(data)
        # debug

        if self.state is None:
            # Wait for the EDID header
            if self.cnt >= OFF_VENDOR:
                if self.cache[-8:] == EDID_HEADER:
                    # Throw away any garbage before the header
                    self.sn = self.sn[-8:]
                    self.cache = self.cache[-8:]
                    self.cnt = 8
                    self.state = 'edid'
                    self.put(self.sn[0][0], es, self.out_ann,
                            [ANN_SECTIONS, ['Header']])
                    self.put(self.sn[0][0], es, self.out_ann,
                            [ANN_FIELDS, ['Header pattern']])
        elif self.state == 'edid':
            if self.cnt == OFF_VERSION:
                self.decode_vid(-10)
                self.decode_pid(-8)
                self.decode_serial(-6)
                self.decode_mfrdate(-2)
                self.put(self.sn[OFF_VENDOR][0], es, self.out_ann,
                        [ANN_SECTIONS, ['Vendor/product']])
            elif self.cnt == OFF_BASIC:
                self.put(self.sn[OFF_VERSION][0], es, self.out_ann,
                        [ANN_SECTIONS, ['EDID Version']])
                self.put(self.sn[OFF_VERSION][0], self.sn[OFF_VERSION][1],
                        self.out_ann, [ANN_FIELDS,
                            ['Version %d' % self.cache[-2]]])
                self.put(self.sn[OFF_VERSION+1][0], self.sn[OFF_VERSION+1][1],
                        self.out_ann, [ANN_FIELDS,
                            ['Revision %d' % self.cache[-1]]])
            elif self.cnt == OFF_CHROM:
                self.put(self.sn[OFF_BASIC][0], es, self.out_ann,
                        [ANN_SECTIONS, ['Basic display']])
                self.decode_basicdisplay(-5)
            elif self.cnt == OFF_EST_TIMING:
                self.put(self.sn[OFF_CHROM][0], es, self.out_ann,
                        [ANN_SECTIONS, ['Color characteristics']])
                self.decode_chromaticity(-10)
            elif self.cnt == OFF_STD_TIMING:
                self.put(self.sn[OFF_EST_TIMING][0], es, self.out_ann,
                        [ANN_SECTIONS, ['Established timings']])
                self.decode_est_timing(-3)
            elif self.cnt == OFF_DET_TIMING:
                self.put(self.sn[OFF_STD_TIMING][0], es, self.out_ann,
                        [ANN_SECTIONS, ['Standard timings']])
                self.decode_std_timing(self.cnt - 16)
            elif self.cnt == OFF_NUM_EXT:
                self.decode_descriptors(-72)
            elif self.cnt == OFF_CHECKSUM:
                self.put(ss, es, self.out_ann,
                    [0, ['Extensions present: %d' % self.cache[self.cnt-1]]])
            elif self.cnt == OFF_CHECKSUM+1:
                checksum = 0
                for i in range(128):
                    checksum += self.cache[i]
                if checksum % 256 == 0:
                    csstr = 'OK'
                else:
                    csstr = 'WRONG!'
                self.put(ss, es, self.out_ann, [0, ['Checksum: %d (%s)' % (
                         self.cache[self.cnt-1], csstr)]])
                self.state = 'extensions'
        elif self.state == 'extensions':
            pass

    def ann_field(self, start, end, annotation):
        self.put(self.sn[start][0], self.sn[end][1],
                 self.out_ann, [ANN_FIELDS, [annotation]])

    def lookup_pnpid(self, pnpid):
        pnpid_file = os.path.join(os.path.dirname(__file__), 'pnpids.txt')
        if os.path.exists(pnpid_file):
            for line in open(pnpid_file).readlines():
                if line.find(pnpid + ';') == 0:
                    return line[4:].strip()
        return ''

    def decode_vid(self, offset):
        pnpid = chr(64 + ((self.cache[offset] & 0x7c) >> 2))
        pnpid += chr(64 + (((self.cache[offset] & 0x03) << 3)
                           | ((self.cache[offset+1] & 0xe0) >> 5)))
        pnpid += chr(64 + (self.cache[offset+1] & 0x1f))
        vendor = self.lookup_pnpid(pnpid)
        if vendor:
            pnpid += ' (%s)' % vendor
        self.ann_field(offset, offset+1, pnpid)

    def decode_pid(self, offset):
        pidstr = 'Product 0x%.2x%.2x' % (self.cache[offset+1], self.cache[offset])
        self.ann_field(offset, offset+1, pidstr)

    def decode_serial(self, offset):
        serialnum = (self.cache[offset+3] << 24) \
                + (self.cache[offset+2] << 16) \
                + (self.cache[offset+1] << 8) \
                + self.cache[offset]
        serialstr = ''
        is_alnum = True
        for i in range(4):
            if not chr(self.cache[offset+3-i]).isalnum():
                is_alnum = False
                break
            serialstr += chr(self.cache[offset+3-i])
        serial = serialstr if is_alnum else str(serialnum)
        self.ann_field(offset, offset+3, 'Serial ' + serial)

    def decode_mfrdate(self, offset):
        datestr = ''
        if self.cache[offset]:
            datestr += 'week %d, ' % self.cache[offset]
        datestr += str(1990 + self.cache[offset+1])
        if datestr:
            self.ann_field(offset, offset+1, 'Manufactured ' + datestr)

    def decode_basicdisplay(self, offset):
        # Video input definition
        vid = self.cache[offset]
        if vid & 0x80:
            # Digital
            self.ann_field(offset, offset, 'Video input: VESA DFP 1.')
        else:
            # Analog
            sls = (vid & 60) >> 5
            self.ann_field(offset, offset, 'Signal level standard: %.2x' % sls)
            if vid & 0x10:
                self.ann_field(offset, offset, 'Blank-to-black setup expected')
            syncs = ''
            if vid & 0x08:
                syncs += 'separate syncs, '
            if vid & 0x04:
                syncs += 'composite syncs, '
            if vid & 0x02:
                syncs += 'sync on green, '
            if vid & 0x01:
                syncs += 'Vsync serration required, '
            if syncs:
                self.ann_field(offset, offset, 'Supported syncs: %s' % syncs[:-2])
        # Max horizontal/vertical image size
        if self.cache[offset+1] != 0 and self.cache[offset+2] != 0:
            # Projectors have this set to 0
            sizestr = '%dx%dcm' % (self.cache[offset+1], self.cache[offset+2])
            self.ann_field(offset+1, offset+2, 'Physical size: ' + sizestr)
        # Display transfer characteristic (gamma)
        if self.cache[offset+3] != 0xff:
            gamma = (self.cache[offset+3] + 100) / 100
            self.ann_field(offset+3, offset+3, 'Gamma: %1.2f' % gamma)
        # Feature support
        fs = self.cache[offset+4]
        dpms = ''
        if fs & 0x80:
            dpms += 'standby, '
        if fs & 0x40:
            dpms += 'suspend, '
        if fs & 0x20:
            dpms += 'active off, '
        if dpms:
            self.ann_field(offset+4, offset+4, 'DPMS support: %s' % dpms[:-2])
        dt = (fs & 0x18) >> 3
        dtstr = ''
        if dt == 0:
            dtstr = 'Monochrome'
        elif dt == 1:
            dtstr = 'RGB color'
        elif dt == 2:
            dtstr = 'non-RGB multicolor'
        if dtstr:
            self.ann_field(offset+4, offset+4, 'Display type: %s' % dtstr)
        if fs & 0x04:
            self.ann_field(offset+4, offset+4, 'Color space: standard sRGB')
        # Save this for when we decode the first detailed timing descriptor
        self.have_preferred_timing = (fs & 0x02) == 0x02
        if fs & 0x01:
            gft = ''
        else:
            gft = 'not '
        self.ann_field(offset+4, offset+4,
                       'Generalized timing formula: %ssupported' % gft)

    def convert_color(self, value):
        # Convert from 10-bit packet format to float
        outval = 0.0
        for i in range(10):
            if value & 0x01:
                outval += 2 ** -(10-i)
            value >>= 1
        return outval

    def decode_chromaticity(self, offset):
        redx = (self.cache[offset+2] << 2) + ((self.cache[offset] & 0xc0) >> 6)
        redy = (self.cache[offset+3] << 2) + ((self.cache[offset] & 0x30) >> 4)
        self.ann_field(offset, offset+9, 'Chromacity red: X %1.3f, Y %1.3f' % (
                       self.convert_color(redx), self.convert_color(redy)))

        greenx = (self.cache[offset+4] << 2) + ((self.cache[offset] & 0x0c) >> 6)
        greeny = (self.cache[offset+5] << 2) + ((self.cache[offset] & 0x03) >> 4)
        self.ann_field(offset, offset+9, 'Chromacity green: X %1.3f, Y %1.3f' % (
                       self.convert_color(greenx), self.convert_color(greeny)))

        bluex = (self.cache[offset+6] << 2) + ((self.cache[offset+1] & 0xc0) >> 6)
        bluey = (self.cache[offset+7] << 2) + ((self.cache[offset+1] & 0x30) >> 4)
        self.ann_field(offset, offset+9, 'Chromacity blue: X %1.3f, Y %1.3f' % (
                       self.convert_color(bluex), self.convert_color(bluey)))

        whitex = (self.cache[offset+8] << 2) + ((self.cache[offset+1] & 0x0c) >> 6)
        whitey = (self.cache[offset+9] << 2) + ((self.cache[offset+1] & 0x03) >> 4)
        self.ann_field(offset, offset+9, 'Chromacity white: X %1.3f, Y %1.3f' % (
                       self.convert_color(whitex), self.convert_color(whitey)))

    def decode_est_timing(self, offset):
        # Pre-EDID modes
        bitmap = (self.cache[offset] << 9) \
            + (self.cache[offset+1] << 1) \
            + ((self.cache[offset+2] & 0x80) >> 7)
        modestr = ''
        for i in range(17):
                if bitmap & (1 << (16-i)):
                    modestr += est_modes[i] + ', '
        if modestr:
            self.ann_field(offset, offset+2,
                           'Supported established modes: %s' % modestr[:-2])

    def decode_std_timing(self, offset):
        modestr = ''
        for i in range(0, 16, 2):
            if self.cache[offset+i] == 0x01 and self.cache[offset+i+1] == 0x01:
                # Unused field
                continue
            x = (self.cache[offset+i] + 31) * 8
            ratio = (self.cache[offset+i+1] & 0xc0) >> 6
            ratio_x, ratio_y = xy_ratio[ratio]
            y = x / ratio_x * ratio_y
            refresh = (self.cache[offset+i+1] & 0x3f) + 60
            modestr += '%dx%d@%dHz, ' % (x, y, refresh)
        if modestr:
            self.ann_field(offset, offset + 15,
                    'Supported standard modes: %s' % modestr[:-2])

    def decode_detailed_timing(self, offset):
        if offset == -72 and self.have_preferred_timing:
            # Only on first detailed timing descriptor
            section = 'Preferred'
        else:
            section = 'Detailed'
        section += ' timing descriptor'
        self.put(self.sn[offset][0], self.sn[offset+17][1],
             self.out_ann, [ANN_SECTIONS, [section]])

        pixclock = float((self.cache[offset+1] << 8) + self.cache[offset]) / 100
        self.ann_field(offset, offset+1, 'Pixel clock: %.2f MHz' % pixclock)

        horiz_active = ((self.cache[offset+4] & 0xf0) << 4) + self.cache[offset+2]
        self.ann_field(offset+2, offset+4, 'Horizontal active: %d' % horiz_active)

        horiz_blank = ((self.cache[offset+4] & 0x0f) << 8) + self.cache[offset+3]
        self.ann_field(offset+2, offset+4, 'Horizontal blanking: %d' % horiz_blank)

        vert_active = ((self.cache[offset+7] & 0xf0) << 4) + self.cache[offset+5]
        self.ann_field(offset+5, offset+7, 'Vertical active: %d' % vert_active)

        vert_blank = ((self.cache[offset+7] & 0x0f) << 8) + self.cache[offset+6]
        self.ann_field(offset+5, offset+7, 'Vertical blanking: %d' % vert_blank)

        horiz_sync_off = ((self.cache[offset+11] & 0xc0) << 2) + self.cache[offset+8]
        self.ann_field(offset+8, offset+11, 'Horizontal sync offset: %d' % horiz_sync_off)

        horiz_sync_pw = ((self.cache[offset+11] & 0x30) << 4) + self.cache[offset+9]
        self.ann_field(offset+8, offset+11, 'Horizontal sync pulse width: %d' % horiz_sync_pw)

        vert_sync_off = ((self.cache[offset+11] & 0x0c) << 2) \
                    + ((self.cache[offset+10] & 0xf0) >> 4)
        self.ann_field(offset+8, offset+11, 'Vertical sync offset: %d' % vert_sync_off)

        vert_sync_pw = ((self.cache[offset+11] & 0x03) << 4) \
                    + (self.cache[offset+10] & 0x0f)
        self.ann_field(offset+8, offset+11, 'Vertical sync pulse width: %d' % vert_sync_pw)

        horiz_size = ((self.cache[offset+14] & 0xf0) << 4) + self.cache[offset+12]
        vert_size = ((self.cache[offset+14] & 0x0f) << 8) + self.cache[offset+13]
        self.ann_field(offset+12, offset+14, 'Physical size: %dx%dmm' % (horiz_size, vert_size))

        horiz_border = self.cache[offset+15]
        self.ann_field(offset+15, offset+15, 'Horizontal border: %d pixels' % horiz_border)
        vert_border = self.cache[offset+16]
        self.ann_field(offset+16, offset+16, 'Vertical border: %d lines' % vert_border)

        features = 'Flags: '
        if self.cache[offset+17] & 0x80:
            features += 'interlaced, '
        stereo = (self.cache[offset+17] & 0x60) >> 5
        if stereo:
            if self.cache[offset+17] & 0x01:
                features += '2-way interleaved stereo ('
                features += ['right image on even lines',
                             'left image on even lines',
                             'side-by-side'][stereo-1]
                features += '), '
            else:
                features += 'field sequential stereo ('
                features += ['right image on sync=1', 'left image on sync=1',
                             '4-way interleaved'][stereo-1]
                features += '), '
        sync = (self.cache[offset+17] & 0x18) >> 3
        sync2 = (self.cache[offset+17] & 0x06) >> 1
        posneg = ['negative', 'positive']
        features += 'sync type '
        if sync == 0x00:
            features += 'analog composite (serrate on RGB)'
        elif sync == 0x01:
            features += 'bipolar analog composite (serrate on RGB)'
        elif sync == 0x02:
            features += 'digital composite (serrate on composite polarity ' \
                        + (posneg[sync2 & 0x01]) + ')'
        elif sync == 0x03:
            features += 'digital separate ('
            features += 'Vsync polarity ' + (posneg[(sync2 & 0x02) >> 1])
            features += ', Hsync polarity ' + (posneg[sync2 & 0x01])
            features += ')'
        features += ', '
        self.ann_field(offset+17, offset+17, features[:-2])

    def decode_descriptor(self, offset):
        tag = self.cache[offset+3]
        if tag == 0xff:
            # Monitor serial number
            self.put(self.sn[offset][0], self.sn[offset+17][1], self.out_ann,
                     [ANN_SECTIONS, ['Serial number']])
            text = bytes(self.cache[offset+5:][:13]).decode(encoding='cp437', errors='replace')
            self.ann_field(offset, offset+17, text.strip())
        elif tag == 0xfe:
            # Text
            self.put(self.sn[offset][0], self.sn[offset+17][1], self.out_ann,
                     [ANN_SECTIONS, ['Text']])
            text = bytes(self.cache[offset+5:][:13]).decode(encoding='cp437', errors='replace')
            self.ann_field(offset, offset+17, text.strip())
        elif tag == 0xfc:
            # Monitor name
            self.put(self.sn[offset][0], self.sn[offset+17][1], self.out_ann,
                     [ANN_SECTIONS, ['Monitor name']])
            text = bytes(self.cache[offset+5:][:13]).decode(encoding='cp437', errors='replace')
            self.ann_field(offset, offset+17, text.strip())
        elif tag == 0xfd:
            # Monitor range limits
            self.put(self.sn[offset][0], self.sn[offset+17][1], self.out_ann,
                     [ANN_SECTIONS, ['Monitor range limits']])
            self.ann_field(offset+5, offset+5, 'Minimum vertical rate: %dHz' %
                           self.cache[offset+5])
            self.ann_field(offset+6, offset+6, 'Maximum vertical rate: %dHz' %
                           self.cache[offset+6])
            self.ann_field(offset+7, offset+7, 'Minimum horizontal rate: %dkHz' %
                           self.cache[offset+7])
            self.ann_field(offset+8, offset+8, 'Maximum horizontal rate: %dkHz' %
                           self.cache[offset+8])
            self.ann_field(offset+9, offset+9, 'Maximum pixel clock: %dMHz' %
                           (self.cache[offset+9] * 10))
            if self.cache[offset+10] == 0x02:
                # Secondary GTF curve supported
                self.ann_field(offset+10, offset+17, 'Secondary timing formula supported')
        elif tag == 0xfb:
            # Additional color point data
            self.put(self.sn[offset][0], self.sn[offset+17][1], self.out_ann,
                     [ANN_SECTIONS, ['Additional color point data']])
        elif tag == 0xfa:
            # Additional standard timing definitions
            self.put(self.sn[offset][0], self.sn[offset+17][1], self.out_ann,
                     [ANN_SECTIONS, ['Additional standard timing definitions']])
        else:
            self.put(self.sn[offset][0], self.sn[offset+17][1], self.out_ann,
                     [ANN_SECTIONS, ['Unknown descriptor']])

    def decode_descriptors(self, offset):
        # 4 consecutive 18-byte descriptor blocks
        for i in range(offset, 0, 18):
            if self.cache[i] != 0 and self.cache[i+1] != 0:
                self.decode_detailed_timing(i)
            else:
                if self.cache[i+2] == 0 or self.cache[i+4] == 0:
                    self.decode_descriptor(i)
