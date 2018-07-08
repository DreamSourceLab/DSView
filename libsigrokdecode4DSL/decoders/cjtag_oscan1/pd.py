##
## Copyright (C) 2018 Sebastien Riou
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

jtag_states = [
        # Intro "tree"
        'TEST-LOGIC-RESET', 'RUN-TEST/IDLE',
        # DR "tree"
        'SELECT-DR-SCAN', 'CAPTURE-DR', 'UPDATE-DR', 'PAUSE-DR',
        'SHIFT-DR', 'EXIT1-DR', 'EXIT2-DR',
        # IR "tree"
        'SELECT-IR-SCAN', 'CAPTURE-IR', 'UPDATE-IR', 'PAUSE-IR',
        'SHIFT-IR', 'EXIT1-IR', 'EXIT2-IR',
]

oscan1_phases = ['nTDI','TMS','TDO']

class Decoder(srd.Decoder):
    api_version = 2
    id = 'cjtag_oscan1'
    name = 'CJTAG OSCAN1'
    longname = 'Joint Test Action Group (IEEE 1149.7 OSCAN1)'
    desc = 'Protocol for testing, debugging, and flashing ICs.'
    license = 'gplv2+'
    inputs = ['logic']
    outputs = ['jtag']
    channels = (
        {'id': 'tck',  'name': 'TCK',  'desc': 'Test clock'},
        {'id': 'tms',  'name': 'TMS',  'desc': 'Test mode select'},
    )
    annotations = tuple([tuple([s.lower(), s]) for s in oscan1_phases]) + tuple([tuple([s.lower(), s]) for s in jtag_states])
    others = ( \
        ('bit-tdi', 'Bit (TDI)'),
        ('bit-tdo', 'Bit (TDO)'),
        ('bitstring-tdi', 'Bitstring (TDI)'),
        ('bitstring-tdo', 'Bitstring (TDO)'),
    )
    annotation_rows = (
        # ('bits-tdi', 'Bits (TDI)', (16,)),
        # ('bits-tdo', 'Bits (TDO)', (17,)),
        # ('bitstrings-tdi', 'Bitstring (TDI)', (18,)),
        # ('bitstrings-tdo', 'Bitstring (TDO)', (19,)),
        ('oscan1-phase', 'OSCAN1 phase', tuple(range(0,0+3)) ),
        ('states', 'States', tuple(range(3,3+15+1))),

    )

    def __init__(self):
        self.state = 'RUN-TEST/IDLE'
        self.phase = 'nTDI'
        self.oldstate = None
        self.oldpins = (-1, -1, -1, -1)
        self.oldtck = -1
        self.bits_tdi = []
        self.bits_tdo = []
        self.bits_samplenums_tdi = []
        self.bits_samplenums_tdo = []
        self.samplenum = 0
        self.ss_item = self.es_item = None
        self.ss_bitstring = self.es_bitstring = None
        self.last_clock_samplenum = None
        self.saved_item = None
        self.first = True
        self.first_bit = True
        self.bits_cnt = 0
        self.data_ready = False

    def start(self):
        self.out_python = self.register(srd.OUTPUT_PYTHON)
        self.out_ann = self.register(srd.OUTPUT_ANN)

    def putx(self, data):
        self.put(self.ss_item, self.es_item, self.out_ann, data)

    def putp(self, data):
        self.put(self.ss_item, self.es_item, self.out_python, data)

    def putx_bs(self, data):
        self.put(self.ss_bitstring, self.es_bitstring, self.out_ann, data)

    def putp_bs(self, data):
        self.put(self.ss_bitstring, self.es_bitstring, self.out_python, data)

    def advance_state_machine(self, tms):
        self.oldstate = self.state

        # Intro "tree"
        if self.state == 'TEST-LOGIC-RESET':
            # self.state = 'TEST-LOGIC-RESET' if (tms) else 'RUN-TEST/IDLE'
            # if we reach this state we are not in OSCAN1 anymore. Since we don't handle anything else we stay in this state to show clearly the failure
            self.state = 'TEST-LOGIC-RESET'
        elif self.state == 'RUN-TEST/IDLE':
            self.state = 'SELECT-DR-SCAN' if (tms) else 'RUN-TEST/IDLE'

        # DR "tree"
        elif self.state == 'SELECT-DR-SCAN':
            self.state = 'SELECT-IR-SCAN' if (tms) else 'CAPTURE-DR'
        elif self.state == 'CAPTURE-DR':
            self.state = 'EXIT1-DR' if (tms) else 'SHIFT-DR'
        elif self.state == 'SHIFT-DR':
            self.state = 'EXIT1-DR' if (tms) else 'SHIFT-DR'
        elif self.state == 'EXIT1-DR':
            self.state = 'UPDATE-DR' if (tms) else 'PAUSE-DR'
        elif self.state == 'PAUSE-DR':
            self.state = 'EXIT2-DR' if (tms) else 'PAUSE-DR'
        elif self.state == 'EXIT2-DR':
            self.state = 'UPDATE-DR' if (tms) else 'SHIFT-DR'
        elif self.state == 'UPDATE-DR':
            self.state = 'SELECT-DR-SCAN' if (tms) else 'RUN-TEST/IDLE'

        # IR "tree"
        elif self.state == 'SELECT-IR-SCAN':
            self.state = 'TEST-LOGIC-RESET' if (tms) else 'CAPTURE-IR'
        elif self.state == 'CAPTURE-IR':
            self.state = 'EXIT1-IR' if (tms) else 'SHIFT-IR'
        elif self.state == 'SHIFT-IR':
            self.state = 'EXIT1-IR' if (tms) else 'SHIFT-IR'
        elif self.state == 'EXIT1-IR':
            self.state = 'UPDATE-IR' if (tms) else 'PAUSE-IR'
        elif self.state == 'PAUSE-IR':
            self.state = 'EXIT2-IR' if (tms) else 'PAUSE-IR'
        elif self.state == 'EXIT2-IR':
            self.state = 'UPDATE-IR' if (tms) else 'SHIFT-IR'
        elif self.state == 'UPDATE-IR':
            self.state = 'SELECT-DR-SCAN' if (tms) else 'RUN-TEST/IDLE'

    def handle_rising_tck_edge(self, tck, tms):

        if self.phase == 'nTDI':
            self.tdi = 1-tms
            if self.first:
                # Save the start sample and item for later (no output yet).
                self.ss_item = self.samplenum
                self.first = False
        elif self.phase == 'TMS':
            self.tms = tms
        elif self.phase == 'TDO':
            self.tdo = tms
            self.advance_state_machine(self.tms)

            # Output the saved item (from the last CLK edge to the current).
            self.es_item = self.samplenum
            if self.ss_item is not None:
                # Output the old state (from last rising TCK edge to current one).
                self.putx([3+jtag_states.index(self.oldstate), [self.oldstate]])
                # self.putp(['NEW STATE', self.state])

            self.ss_item = self.samplenum

        if 0:
            # Upon SHIFT-IR/SHIFT-DR collect the current TDI/TDO values.
            if self.state.startswith('SHIFT-'):
                if self.bits_cnt > 0:
                    if self.bits_cnt == 1:
                        self.ss_bitstring = self.samplenum

                    if self.bits_cnt > 1:
                        self.putx([16, [str(self.bits_tdi[0])]])
                        self.putx([17, [str(self.bits_tdo[0])]])
                        # Use self.samplenum as ES of the previous bit.
                        self.bits_samplenums_tdi[0][1] = self.samplenum
                        self.bits_samplenums_tdo[0][1] = self.samplenum

                    self.bits_tdi.insert(0, tdi)
                    self.bits_tdo.insert(0, tdo)

                # Use self.samplenum as SS of the current bit.
                self.bits_samplenums_tdi.insert(0, [self.samplenum, -1])
                self.bits_samplenums_tdo.insert(0, [self.samplenum, -1])

                self.bits_cnt = self.bits_cnt + 1

            # Output all TDI/TDO bits if we just switched from SHIFT-* to EXIT1-*.
            if self.oldstate.startswith('SHIFT-') and \
               self.state.startswith('EXIT1-'):

                #self.es_bitstring = self.samplenum
                if self.bits_cnt > 0:
                    if self.bits_cnt == 1:  # Only shift one bit
                        self.ss_bitstring = self.samplenum
                        self.bits_tdi.insert(0, tdi)
                        self.bits_tdo.insert(0, tdo)
                        ## Use self.samplenum as SS of the current bit.
                        self.bits_samplenums_tdi.insert(0, [self.samplenum, -1])
                        self.bits_samplenums_tdo.insert(0, [self.samplenum, -1])
                    else:
                        ### ----------------------------------------------------------------
                        self.putx([16, [str(self.bits_tdi[0])]])
                        self.putx([17, [str(self.bits_tdo[0])]])
                        ### Use self.samplenum as ES of the previous bit.
                        self.bits_samplenums_tdi[0][1] = self.samplenum
                        self.bits_samplenums_tdo[0][1] = self.samplenum

                        self.bits_tdi.insert(0, tdi)
                        self.bits_tdo.insert(0, tdo)

                        ## Use self.samplenum as SS of the current bit.
                        self.bits_samplenums_tdi.insert(0, [self.samplenum, -1])
                        self.bits_samplenums_tdo.insert(0, [self.samplenum, -1])
                        ## ----------------------------------------------------------------

                    self.data_ready = True

                self.first_bit = True
                self.bits_cnt = 0
            if self.oldstate.startswith('EXIT'):
                if self.data_ready:
                    self.data_ready = False
                    self.es_bitstring = self.samplenum
                    t = self.state[-2:] + ' TDI'
                    b = ''.join(map(str, self.bits_tdi))
                    h = ' (0x%X' % int('0b' + b, 2) + ')'
                    s = t + ': ' + h + ', ' + str(len(self.bits_tdi)) + ' bits' #b +
                    self.putx_bs([18, [s]])
                    self.bits_samplenums_tdi[0][1] = self.samplenum # ES of last bit.
                    self.putp_bs([t, [b, self.bits_samplenums_tdi]])
                    self.putx([16, [str(self.bits_tdi[0])]]) # Last bit.
                    self.bits_tdi = []
                    self.bits_samplenums_tdi = []

                    t = self.state[-2:] + ' TDO'
                    b = ''.join(map(str, self.bits_tdo))
                    h = ' (0x%X' % int('0b' + b, 2) + ')'
                    s = t + ': ' + h + ', ' + str(len(self.bits_tdo)) + ' bits' #+ b
                    self.putx_bs([19, [s]])
                    self.bits_samplenums_tdo[0][1] = self.samplenum # ES of last bit.
                    self.putp_bs([t, [b, self.bits_samplenums_tdo]])
                    self.putx([17, [str(self.bits_tdo[0])]]) # Last bit.
                    self.bits_tdo = []
                    self.bits_samplenums_tdo = []


    def handle_falling_tck_edge(self, tck, tms):

        if self.phase == 'nTDI':
            next_phase = 'TMS'
        elif self.phase == 'TMS':
            next_phase = 'TDO'
        elif self.phase == 'TDO':
            next_phase = 'nTDI'

        if self.last_clock_samplenum is not None:
            self.put(self.last_clock_samplenum, self.samplenum, self.out_ann, [0+oscan1_phases.index(self.phase), [self.phase]])

        self.last_clock_samplenum = self.samplenum
        self.phase = next_phase

    def decode(self, ss, es, logic):
        for (self.samplenum, pins) in logic:
            logic.logic_mask = 0b11
            logic.cur_pos = self.samplenum
            logic.edge_index = -1

            if self.last_clock_samplenum is None:
                self.last_clock_samplenum = self.samplenum
            elif self.last_clock_samplenum >= self.samplenum:
                continue

            # If none of the pins changed, there's nothing to do.
            if self.oldpins == pins:
                continue

            # Store current pin values for the next round.
            self.oldpins = pins

            # Get individual pin values into local variables.
            # Unused channels will have a value of > 1.
            ( tck, tms) = pins

            # We only care about TCK edges (either rising or falling).
            if (self.oldtck == tck):
                continue

            # Store start/end sample for later usage.
            self.ss, self.es = ss, es

            if (self.oldtck == 0 and tck == 1):
                self.handle_rising_tck_edge( tck, tms)
            elif (self.oldtck == 1 and tck == 0):
                self.handle_falling_tck_edge( tck, tms)

            self.oldtck = tck
