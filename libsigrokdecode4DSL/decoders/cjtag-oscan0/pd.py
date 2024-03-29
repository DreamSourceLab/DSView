##
## This file is part of the libsigrokdecode project.
##
## Copyright (C) 2012-2015 Uwe Hermann <uwe@hermann-uwe.de>
## Copyright (C) 2019 DreamSourceLab <support@dreamsourcelab.com>
## Copyright (C) 2019 Zhiyuan Wan   <dv.xw@qq.com>
## Copyright (C) 2019 Kongou Hikari <hikari@iloli.bid>
##
## Version: 
## Modified by Shiqiu Nie(369614718@qq.com)
## Date: 2017-01-11
## Descript: 
##  1. 2017-01-10 Fixed TDI/TDO data decode, when JTAG TAP run into 
##                SHIFT-IR/SHIFT-DR status,the first bit is not a valid bit.
##  2. 2017-01-11 Fixed decode when shift only one bit.
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

'''
OUTPUT_PYTHON format:

Packet:
[<ptype>, <pdata>]

<ptype>:
 - 'NEW STATE': <pdata> is the new state of the JTAG state machine.
   Valid values: 'TEST-LOGIC-RESET', 'RUN-TEST/IDLE', 'SELECT-DR-SCAN',
   'CAPTURE-DR', 'SHIFT-DR', 'EXIT1-DR', 'PAUSE-DR', 'EXIT2-DR', 'UPDATE-DR',
   'SELECT-IR-SCAN', 'CAPTURE-IR', 'SHIFT-IR', 'EXIT1-IR', 'PAUSE-IR',
   'EXIT2-IR', 'UPDATE-IR'.
 - 'IR TDI': Bitstring that was clocked into the IR register.
 - 'IR TDO': Bitstring that was clocked out of the IR register.
 - 'DR TDI': Bitstring that was clocked into the DR register.
 - 'DR TDO': Bitstring that was clocked out of the DR register.

All bitstrings are a list consisting of two items. The first is a sequence
of '1' and '0' characters (the right-most character is the LSB. Example:
'01110001', where 1 is the LSB). The second item is a list of ss/es values
for each bit that is in the bitstring.
'''

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

class Decoder(srd.Decoder):
    api_version = 3
    id = 'cjtag_oscan1'
    name = 'cJTAG OScan1'
    longname = 'Compact Joint Test Action Group (IEEE 1149.7)'
    desc = 'Protocol for testing, debugging, and flashing ICs, Now this plugin has no ZBS support, it only supports Oscan1 format.'
    license = 'gplv2+'
    inputs = ['logic']
    outputs = ['jtag']
    tags = ['Debug/trace']
    channels = (
        {'id': 'tdi',  'name': 'TDI',  'desc': 'Test data input', 'idn':'dec_cjtag_oscan0_chan_tdi'},
        {'id': 'tdo',  'name': 'TDO',  'desc': 'Test data output', 'idn':'dec_cjtag_oscan0_chan_tdo'},
        {'id': 'tck',  'name': 'TCK',  'desc': 'Test clock', 'idn':'dec_cjtag_oscan0_chan_tck'},
        {'id': 'tms',  'name': 'TMS',  'desc': 'Test mode select', 'idn':'dec_cjtag_oscan0_chan_tms'},
    )
    optional_channels = (
        {'id': 'trst', 'name': 'TRST#', 'desc': 'Test reset', 'idn':'dec_cjtag_oscan0_opt_chan_trst'},
        {'id': 'srst', 'name': 'SRST#', 'desc': 'System reset', 'idn':'dec_cjtag_oscan0_opt_chan_srst'},
        {'id': 'rtck', 'name': 'RTCK',  'desc': 'Return clock signal', 'idn':'dec_cjtag_oscan0_opt_chan_rtck'},
    )
    annotations = tuple([tuple([s.lower(), s]) for s in jtag_states]) + ( \
        ('bit-tdi', 'Bit (TDI)'),
        ('bit-tdo', 'Bit (TDO)'),
        ('bitstring-tdi', 'Bitstring (TDI)'),
        ('bitstring-tdo', 'Bitstring (TDO)'),
        ('bit-tms', 'Bit (TMS)'),
        ('state-tapc', 'TAPC State'),
    )
    annotation_rows = (
        ('bits-tdi', 'Bits (TDI)', (16,)),
        ('bits-tdo', 'Bits (TDO)', (17,)),
        ('bitstrings-tdi', 'Bitstring (TDI)', (18,)),
        ('bitstrings-tdo', 'Bitstring (TDO)', (19,)),
        ('bit-tms', 'Bit (TMS)', (20,)),
        ('state-tapc', 'TAPC State', (21,)),
        ('states', 'States', tuple(range(15 + 1))),
    )

    def __init__(self):
        self.reset()

    def reset(self):
        self.state = 'TEST-LOGIC-RESET'
        # self.state = 'RUN-TEST/IDLE'
        self.cjtagstate = '4-WIRE'
        self.oldcjtagstate = None
        self.escape_edges = 0
        self.oaclen = 0
        self.oldtms = 0
        self.oacp = 0
        self.oscan1cycle = 0
        self.oldstate = None
        self.bits_tdi = []
        self.bits_tdo = []
        self.bits_samplenums_tdi = []
        self.bits_samplenums_tdo = []
        self.ss_item = self.es_item = None
        self.ss_bitstring = self.es_bitstring = None
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

        if self.cjtagstate.startswith("CJTAG-"):
            self.oacp = self.oacp + 1
            if (self.oacp > 4 and self.oaclen == 12):
                self.cjtagstate = 'CJTAG-EC'

            if (self.oacp == 8 and tms == 0):
                self.oaclen = 36
            if (self.oacp > 8 and self.oaclen == 36):
                self.cjtagstate = 'CJTAG-SPARE'
            if (self.oacp > 13 and self.oaclen == 36):
                self.cjtagstate = 'CJTAG-TPDEL'
            if (self.oacp > 16 and self.oaclen == 36):
                self.cjtagstate = 'CJTAG-TPREV'
            if (self.oacp > 18 and self.oaclen == 36):
                self.cjtagstate = 'CJTAG-TPST'
            if (self.oacp > 23 and self.oaclen == 36):
                self.cjtagstate = 'CJTAG-RDYC'
            if (self.oacp > 25 and self.oaclen == 36):
                self.cjtagstate = 'CJTAG-DLYC'
            if (self.oacp > 27 and self.oaclen == 36):
                self.cjtagstate = 'CJTAG-SCNFMT'

            if (self.oacp > 8 and self.oaclen == 12):
                self.cjtagstate = 'CJTAG-CP'
            if (self.oacp > 32 and self.oaclen == 36):
                self.cjtagstate = 'CJTAG-CP'

            if (self.oacp > self.oaclen):
                self.cjtagstate = 'OSCAN1'
                self.oscan1cycle = 1
                self.state = 'TEST-LOGIC-RESET' # Because Nuclei cJTAG device asserts a reset during cJTAG online activating.

        else :
            # Intro "tree"
            if self.state == 'TEST-LOGIC-RESET':
                self.state = 'TEST-LOGIC-RESET' if (tms) else 'RUN-TEST/IDLE'
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
            
    def handle_rising_tck_edge(self, tdi, tdo, tck, tms, trst, srst, rtck):
        # Rising TCK edges always advance the state machine.

        self.advance_state_machine(tms)
        
        if self.first:
            # Save the start sample and item for later (no output yet).
            self.ss_item = self.samplenum
            self.first = False
        else:
            # Output the saved item (from the last CLK edge to the current).
            self.es_item = self.samplenum
            # Output the old state (from last rising TCK edge to current one).
            self.putx([jtag_states.index(self.oldstate), [self.oldstate]])
            self.putp(['NEW STATE', self.state])
            self.putx([21, [self.oldcjtagstate]])
            if(self.oldcjtagstate.startswith("CJTAG-")):
                self.putx([20, [str(self.oldtms)]])
            #self.putx([20, [str(tms)]])
            #self.putx([16, [str(tdi)]])
            #self.putx([17, [str(tdo)]])
        self.oldtms = tms
        # Upon SHIFT-IR/SHIFT-DR collect the current TDI/TDO values.
        if self.state.startswith('SHIFT-'):
            #if self.first_bit:
                #self.ss_bitstring = self.samplenum
            #    self.first_bit = False
                
            #else:
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
        if self.oldstate.startswith('EXIT'):# and \
           #self.state.startswith('PAUSE-'):
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

            #self.first_bit = True
            #self.bits_cnt = 0

            #self.ss_bitstring = self.samplenum
        
        self.ss_item = self.samplenum

    def handle_tms_edge(self, tck, tms):
        self.escape_edges = self.escape_edges + 1
        #print ("Detected escape sequence " + str(self.escape_edges))
        #self.es_item = self.samplenum
        #self.putx([20, [str(self.escape_edges)]])
    def handle_tapc_state(self, tck, tms):
        self.oldcjtagstate = self.cjtagstate

        if self.escape_edges >= 8:
            self.cjtagstate = '4-WIRE'
        if self.escape_edges == 6 : #| self.escape_edges == 7:
            self.cjtagstate = 'CJTAG-OAC'
            self.oacp = 0
            self.oaclen = 12

        self.escape_edges = 0

    def decode(self):
        tdi_real = 0
        tms_real = 0
        tdo_real = 0

        while True:
            # Wait for a rising edge on TCK.
            
            (tdi, tdo, tck, tms, trst, srst, rtck) = self.wait({2: 'r'})
            self.handle_tapc_state(tck, tms)

            if(self.cjtagstate == 'OSCAN1'):
                if(self.oscan1cycle == 0): #nTDI
                    if(tms == 0):
                        tdi_real = 1
                    else:
                        tdi_real = 0
                    self.oscan1cycle = 1
                elif(self.oscan1cycle == 1): #TMS
                    tms_real = tms
                    self.oscan1cycle = 2
                elif(self.oscan1cycle == 2): #TDO
                    tdo_real = tms
                    self.handle_rising_tck_edge(tdi_real, tdo_real, tck, tms_real, trst, srst, rtck)
                    self.oscan1cycle = 0
            else:
                self.handle_rising_tck_edge(tdi, tdo, tck, tms, trst, srst, rtck)

            while (tck == 1):
                (tdi, tdo, tck, tms_n, trst, srst, rtck) = self.wait([{2: 'f'}, {3: 'e'}]) 
                if(tms_n != tms):
                    tms = tms_n
                    self.handle_tms_edge(tck, tms)



