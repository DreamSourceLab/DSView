##
## This file is part of the libsigrokdecode project.
##
## Copyright (C) 2017 Joel Holdsworth <joel@airwebreathe.org.uk>
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

import sigrokdecode as srd

def disabled_enabled(v):
    return ['Disabled', 'Enabled'][v]

def output_power(v):
    return '%+ddBm' % [-4, -1, 2, 5][v]

regs = {
# reg:   name                        offset width parser
    0: [
        ('FRAC',                          3, 12, None),
        ('INT',                          15, 16, lambda v: 'Not Allowed' if v < 32 else v)
    ],
    1: [
        ('MOD',                           3, 12, None),
        ('Phase',                        15, 12, None),
        ('Prescalar',                    27,  1, lambda v: ['4/5', '8/9'][v]),
        ('Phase Adjust',                 28,  1, lambda v: ['Off', 'On'][v]),
    ],
    2: [
        ('Counter Reset',                 3,  1, disabled_enabled),
        ('Charge Pump Three-State',       4,  1, disabled_enabled),
        ('Power-Down',                    5,  1, disabled_enabled),
        ('PD Polarity',                   6,  1, lambda v: ['Negative', 'Positive'][v]),
        ('LDP',                           7,  1, lambda v: ['10ns', '6ns'][v]),
        ('LDF',                           8,  1, lambda v: ['FRAC-N', 'INT-N'][v]),
        ('Charge Pump Current Setting',   9,  4, lambda v: '%0.2fmA @ 5.1kΩ' %
            [0.31, 0.63, 0.94, 1.25, 1.56, 1.88, 2.19, 2.50,
            2.81, 3.13, 3.44, 3.75, 4.06, 4.38, 4.69, 5.00][v]),
        ('Double Buffer',                13,  1, disabled_enabled),
        ('R Counter',                    14, 10, None),
        ('RDIV2',                        24,  1, disabled_enabled),
        ('Reference Doubler',            25,  1, disabled_enabled),
        ('MUXOUT',                       26,  3, lambda v:
            ['Three-State Output', 'DVdd', 'DGND', 'R Counter Output', 'N Divider Output',
            'Analog Lock Detect', 'Digital Lock Detect', 'Reserved'][v]),
        ('Low Noise and Low Spur Modes', 29,  2, lambda v:
            ['Low Noise Mode', 'Reserved', 'Reserved', 'Low Spur Mode'][v])
    ],
    3: [
        ('Clock Divider',                 3, 12, None),
        ('Clock Divider Mode',           15,  2, lambda v:
            ['Clock Divider Off', 'Fast Lock Enable', 'Resync Enable', 'Reserved'][v]),
        ('CSR Enable',                   18,  1, disabled_enabled),
        ('Charge Cancellation',          21,  1, disabled_enabled),
        ('ABP',                          22,  1, lambda v: ['6ns (FRAC-N)', '3ns (INT-N)'][v]),
        ('Band Select Clock Mode',       23,  1, lambda v: ['Low', 'High'][v])
    ],
    4: [
        ('Output Power',                  3,  2, output_power),
        ('Output Enable',                 5,  1, disabled_enabled),
        ('AUX Output Power',              6,  2, output_power),
        ('AUX Output Select',             8,  1, lambda v: ['Divided Output', 'Fundamental'][v]),
        ('AUX Output Enable',             9,  1, disabled_enabled),
        ('MTLD',                         10,  1, disabled_enabled),
        ('VCO Power-Down',               11,  1, lambda v:
            'VCO Powered ' + ('Down' if v == 1 else 'Up')),
        ('Band Select Clock Divider',    12,  8, None),
        ('RF Divider Select',            20,  3, lambda v: '÷' + str(2**v)),
        ('Feedback Select',              23,  1, lambda v: ['Divided', 'Fundamental'][v]),
    ],
    5: [
        ('LD Pin Mode',                  22,  2, lambda v:
            ['Low', 'Digital Lock Detect', 'Low', 'High'][v])
    ]
}

ANN_REG = 0

class Decoder(srd.Decoder):
    api_version = 2
    id = 'adf435x'
    name = 'ADF435x'
    longname = 'Analog Devices ADF4350/1'
    desc = 'Wideband synthesizer with integrated VCO.'
    license = 'gplv3+'
    inputs = ['spi']
    outputs = ['adf435x']
    annotations = (
        # Sent from the host to the chip.
        ('register', 'Register written to the device'),
    )
    annotation_rows = (
        ('registers', 'Register writes', (ANN_REG,)),
    )

    def __init__(self):
        self.bits = []

    def start(self):
        self.out_ann = self.register(srd.OUTPUT_ANN)

    def decode_bits(self, offset, width):
        return (sum([(1 << i) if self.bits[offset + i][0] else 0 for i in range(width)]),
            (self.bits[offset + width - 1][1], self.bits[offset][2]))

    def decode_field(self, name, offset, width, parser):
        val, pos = self.decode_bits(offset, width)
        self.put(pos[0], pos[1], self.out_ann, [ANN_REG,
            ['%s: %s' % (name, parser(val) if parser else str(val))]])
        return val

    def decode(self, ss, es, data):

        ptype, data1, data2 = data

        if ptype == 'CS-CHANGE':
            if data1 == 1:
                if len(self.bits) == 32:
                    reg_value, reg_pos = self.decode_bits(0, 3)
                    self.put(reg_pos[0], reg_pos[1], self.out_ann, [ANN_REG,
                        ['Register: %d' % reg_value, 'Reg: %d' % reg_value,
                         '[%d]' % reg_value]])
                    if reg_value < len(regs):
                        field_descs = regs[reg_value]
                        for field_desc in field_descs:
                            field = self.decode_field(*field_desc)
                self.bits = []
        if ptype == 'BITS':
            self.bits = data1 + self.bits
