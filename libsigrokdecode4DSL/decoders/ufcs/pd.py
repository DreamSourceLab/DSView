##
## This file is part of the libsigrokdecode project.
##
## Copyright (C) 2023 edison ren <i2tv@qq.com>
## ref from uart & usb_power_delivery 
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

# Control Message type
CTRL_TYPES = {
    0: 'PING',
    1: 'ACK',
    2: 'NCK',
    3: 'ACCEPT',
    4: 'SOFT RESET',
    5: 'POWER READY',
    6: 'GET OUTPUT CAP',
    7: 'GET SOURCE INFO',
    8: 'GET SINK INFO',
    9: 'GET CABLE INFO',
    10: 'GET DEVICE INFO',
    11: 'GET ERROR INFO',
    12: 'DETECT CABLE INFO',
    13: 'START CABLE DETECT',
    14: 'END CABLE DETECT',
    15: 'EXIT UFCS MODE',
}

# Data message type
DATA_TYPES = {
    1: 'OUTPUT CAP',
    2: 'REQUEST',
    3: 'SOURCE INFO',
    4: 'SINK INFO',
    5: 'CABLE INFO',
    6: 'DEVICE INFO',
    7: 'ERROR INFO',
    8: 'CONFIG WATCHDOG',
    9: 'REFUSE',
    10: 'Verify_Request',
    11: 'Verify_Response',
    255: 'Test Request'
}

class Decoder(srd.Decoder):
    api_version = 3
    id = 'ufcs'
    name = 'UFCS'
    longname = 'Universal Fast Charging Specification'
    desc = 'Universal fast charging specification for mobile devices. T/TAF 083-2021. Coding by edison ren 2023.1.25 <i2tv@qq.com>'
    license = 'gplv2+'
    inputs = ['uart']
    outputs = []
    tags = ['PC/Mobile']

    options = (
        {'id': 'fulltext', 'desc': 'Full text decoding of packets',
         'default': 'no', 'values': ('yes', 'no')},
    )
    annotations = (
        ('type', 'Packet Type'),
        ('training', 'Training'),
        ('header', 'Header'),
        ('data', 'Data'),
        ('crc', 'Checksum'),
        ('warnings', 'Warnings'),
        ('src', 'Source Message'),
        ('snk', 'Sink Message'),
        ('payload', 'Payload'),
        ('text', 'Plain text'),
        ('cable', 'Cable Message'),
        ('reserved', 'Reserved'),
        
    )
    annotation_rows = (
       ('phase', 'Parts', (1, 2, 3, 4,)),
       ('payload', 'Payload', (8,)),
       ('type', 'Type', (0, 6, 7, 10, 11)),
       ('warnings', 'Warnings', (5,)),
       ('text', 'Full text', (9,)),
    )

    def __init__(self):
        self.reset()

    def reset(self):
        self.ss_block = None    # start pos with samles for show
        self.es_block = None    # end pos with samples for show
        self.dataidx = 0        # data pos with u8 in packet 
        self.datapkt = []       # data buffer for packet, 4:128
        self.head = []          # header 2*u8 
        self.bytepos = []       # (s, e) pos of byte (start, end) 
        self.text = ''          # info string 
        self.plen = 0;

    def start(self):
        self.out_ann = self.register(srd.OUTPUT_ANN)

    def head_id(self):
        return (self.head[0] >> 1) & 15

    def head_power_role(self):
        return (self.head[0] >> 5) & 7

    def head_rev(self):
        return (self.head[1] >> 3) & 31

    def head_type(self):
        return self.head[2]

    def data_len(self):
	# data_msg count = data[3], ctrl_msg count = 0 
        return self.head[3] if (self.head[1] & 7) == 1 else 0

    def putx(self, s0, s1, data):
        self.put(s0, s1, self.out_ann, data)

    def putwarn(self, longm, shortm):
        self.putx(0, -1, [8, [longm, shortm]])

    def compute_crc8(self):
        CRC_8_POLYNOMIAL = 0x29
        rCRC = 0

        for s in range(self.plen - 1):
            rCRC ^= self.datapkt[s]
            for i in range(8):
                if (rCRC & 0x80):
                    rCRC = (rCRC << 1) ^ CRC_8_POLYNOMIAL
                else:
                    rCRC = (rCRC << 1)

        return (rCRC & 0xff)

    def get_short(self, i):
        k = [self.datapkt[i], self.datapkt[i+1]]
        val = k[0] <<8 | (k[1])
        return val

    def get_word(self, i):
        hi = self.get_short(i)
        lo = self.get_short(i+2)
        return lo | (hi << 16)
    
    def get_dword(self, i):
        hi = self.get_word(i)
        lo = self.get_word(i+4)
        return lo | (hi << 32)
    
    def puthead(self):
        # message reviever, SRC, SNK, Cable
        pwr_role = self.head_power_role()
        if pwr_role == 1:
            ann_type = 6
            role = 'SRC'
        elif pwr_role == 2:
            ann_type = 7
            role = 'SNK'
        elif pwr_role == 3:
            ann_type = 10
            role = 'Cable'
        else:
            ann_type = 11
            role = 'Reserved'

        t = self.head_type()
        
        if self.data_len() == 0:
            if t > 15:
                shortm = 'reserved cmd'
            else:
                shortm = CTRL_TYPES[t]
        else:
            shortm = DATA_TYPES[t] if t in DATA_TYPES else 'DAT???'

        longm = '(r{:d}) {:s}[{:d}]: {:s}'.format(self.head_rev(), role, self.head_id(), shortm)
        self.putx(self.bytepos[0][0], self.bytepos[2][1], [ann_type, [longm, shortm]])
        self.text += longm

    def get_source_cap(self):
        numbpdo = int(self.data_len()) // 8
        strpdo = ''

        for numb in range(numbpdo):
            pdo = self.get_dword(4 + 8 * numb)
            mode = (pdo >> 60) & 15
            step_ma = ((pdo >> 57) & 7) * 10 + 10
            step_mv = ((pdo >> 56) & 1) * 10 + 10
            max_mv = ((pdo >> 40) & 0xffff) * 0.01
            min_mv = ((pdo >> 24) & 0xffff) * 0.01
            max_ma = ((pdo >> 8) & 0xffff) * 0.01
            min_ma = (pdo & 0xff) * 0.01
            strpdo = '[%d] %g/%gV *%gmv %g/%gA *%gma ' \
                    % (mode, min_mv, max_mv, step_mv, min_ma, max_ma, step_ma)

            s0 = self.bytepos[4 + 8 * numb][0]
            s1 = self.bytepos[4 + 8 * numb + 7][1]
            # show data in hex
            self.putx(s0, s1, [3, ['[%d]%08x' % (numb, pdo), 'D%d' % (numb)]])
            # show data in comment
            txt = '(PDO %s)' % strpdo
            self.putx(s0, s1, [8, [txt, txt]])
            # show data in full text string
            self.text += ' - ' + txt

        return self.text

    def get_request(self):
        rdo = self.get_dword(4)
        
        mode = (rdo >> 60) & 15

        curr = (rdo & 0xffff) * 0.01
        volt = ((rdo >> 16) & 0xffff) * 0.01

        s = '%gV %gA' % (volt, curr)

        return rdo, '(PDO #%d) %s' % (mode, s)
       
    def get_src_info(self):
        d = self.get_dword(4)
        #self.putwarn('bbbbbb%08x' % self.get_dword(4), 'aaa')

        it = ((d >> 40 ) & 0xff) - 50
        its = ',internal temp %s' % ('%dC' % it if it > -50 else 'no data')
        
        pt = ((d >> 32 ) & 0xff) - 50
        pts = ',usb port temp %s' % ('%dC' % pt if pt > -50 else 'no data')
        
        curr = (d & 0xffff) * 0.01
        volt = ((d >> 16) & 0xffff) * 0.01

        return d, '(SRC info: %gV %gA %s %s)' % (volt, curr, its, pts)

    def get_snk_info(self):
        d = self.get_dword(4)

        it = ((d >> 40 ) & 0xff) - 50
        its = ',battery temp %s' % ('%dC' % it if it > -50 else 'no data')
        
        pt = ((d >> 32 ) & 0xff) - 50
        pts = ',usb port temp %s' % ('%dC' % pt if pt > -50 else 'no data')
        
        curr = (d & 0xffff) * 0.01
        volt = ((d >> 16) & 0xffff) * 0.01

        return d, '(SNK info: %gV %gA %s %s)' % (volt, curr, its, pts)

    def get_cable_info(self):
        d = self.get_dword(4)

        id = ((d >> 48 ) & 0xffff)
        ids = 'Cable VID %04X' % (id)
        
        emk = ((d >> 32 ) & 0xffff)
        emks = 'Emark VID %04X' % emk
        
        imp = ((d >> 16) & 0xffff)
        imps = 'IMP %d' % imp 

        curr = (d & 0xff)
        volt = ((d >> 8) & 0xff)

        return d, '(%s %s %s %gV %gA)' % (ids, emks, imps, volt, curr)

    def get_device_info(self):
        d = self.get_dword(4)

        id = ((d >> 48 ) & 0xffff)
        ids = 'Device VID %04X' % (id)
        
        pid = ((d >> 32 ) & 0xffff)
        pids = 'Protocol IC VID %04X' % emk
        
        hwv = ((d >> 16) & 0xffff)
        hwvs = 'HW rev%04X' % imp 

        swv = (d & 0xffff) 
        swvs = 'SW rev%04X' % imp 

        return d, '(%s %s %s %s)' % (ids, pids, hwv, swv)

    def get_error_info(self):
        d = self.get_word(4)

        id = ((d >> 15 ) & 0x1ffff)
        ids = 'Reversed %04X' % (id)
        
        e14 = ((d >> 14 ) & 1)
        e14s = 'Watchdog %s' % ('OK' if e14 == 0 else 'NG!')
        
        e13 = ((d >> 13 ) & 1)
        e13s = 'CRC %s' % ('OK' if e13 == 0 else 'NG!')
        
        e12 = ((d >> 12 ) & 1)
        e12s = 'Input %s' % ('OK' if e12 == 0 else 'NG!')
        
        e11 = ((d >> 11 ) & 1)
        e11s = 'Current loss %s' % ('OK' if e11 == 0 else 'NG!')
        
        e10 = ((d >> 10 ) & 1)
        e10s = 'UVP %s' % ('OK' if e10 == 0 else 'NG!')
        
        e9 = ((d >> 9 ) & 1)
        e9s = 'OVP %s' % ('OK' if e9 == 0 else 'NG')
        
        e8 = ((d >> 8 ) & 1)
        e8s = 'D+OVP %s' % ('OK' if e8 == 0 else 'NG')
        
        e7 = ((d >> 7 ) & 1)
        e7s = 'D-OVP %s' % ('OK' if e7 == 0 else 'NG')
        
        e6 = ((d >> 6 ) & 1)
        e6s = 'CCOVP %s' % ('OK' if e6 == 0 else 'NG')
        
        e5 = ((d >> 5 ) & 1)
        e5s = 'Battery OTP %s' % ('OK' if e5 == 0 else 'NG')
        
        e4 = ((d >> 4 ) & 1)
        e4s = 'USB port OTP %s' % ('OK' if e4 == 0 else 'NG')
        
        e3 = ((d >> 3 ) & 1)
        e3s = 'SCP %s' % ('OK' if e3 == 0 else 'NG')
        
        e2 = ((d >> 2 ) & 1)
        e2s = 'OCP %s' % ('OK' if e2 == 0 else 'NG')
        
        e1 = ((d >> 1 ) & 1)
        e1s = 'UVP %s' % ('OK' if e1 == 0 else 'NG')
        
        e0 = ((d >> 0 ) & 1)
        e0s = 'OVP %s' % ('OK' if e0 == 0 else 'NG')
        
        return d, '(%s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s)' % (ids, \
                e14, e13, e12, e11, e10, e9, e8, e7, e6, e5, e4, e3, e2, e1, e0)

    def config_watchdog(self):
        d = self.get_short(4)
        
        return d, 'Watchdog overflow time %dms' % d
    
    def refuse(self):
        d = self.get_word(4)

        rev = ((d >> 28 ) & 0xf)
        revs = ('Reversed %01X' % rev) if rev else ''
        rev2 = ((d >> 19 ) & 0xf)
        revs2 = ('Reversed %01X' % rev2) if rev2 else ''
                
        id = ((d >> 24 ) & 0xf)
        ids = 'MSG #%d' % (id)
        
        t = ((d >> 16 ) & 7)
        ts = ', Type %d' % ts
        
        c = ((d >> 8) & 0xff)
        cs = ', CMD %d' % c

        r1 = ((d >> 0) & 0x7)
        rs1  = ', unknow'      if r1 == 1 else ''
        rs1 += ', not support' if r1 == 2 else ''
        rs1 += ', busy'        if r1 == 3 else ''
        rs1 += ', over range'  if r1 == 4 else ''
        rs1 += ', other'       if r1 == 5 else ''

        return d, 'Refuse why? $s' % (ids + ts + cs + revs +revs2 + rs1)

    def verify_request(self):
        id = self.datapkt[4]
        dhi = self.get_dword(5)
        dlo = self.get_dword(5 + 8) 
        
        return id, 'Verify request key ID %d, random %08x%08x' % (id, dhi, dhl)
        
    def verify_response(self):
        key = self.get_dword(4)
        rand = self.get_word(4+8) 
        
        return id, 'Verify request data %08x, random %04x' % (key, rand)
            
    def decode_data_msg(self):
        dlen = self.data_len()

        # no data, do nothing
        if dlen == 0:
            return

        txt = ''
        t = self.head_type()
                            
        # SRC CAP
        if   t == 1:
            self.get_source_cap()
            # src cap has n(1-15) PDO, putx will be run n times within function,
            # so difference here
            return 
           
        # request
        elif t == 2:
            d, txt = self.get_request()
            
        
        # SRC info
        elif t == 3:
            d, txt = self.get_src_info()

        # SNK info
        elif t == 4:
            d, txt = self.get_snk_info()
            
        # cable info
        elif t == 5:
            d, txt = self.get_cable_info()

        # device info
        elif t == 6:
            d, txt = self.get_device_info()

        # error info
        elif t == 7:
            d, txt = self.get_error_info()

        # config watchdog
        elif t == 8:
            d, txt = self.config_watchdog()

        # refuse
        elif t == 9:
            d, txt = self.refuse()

        # verify request
        elif t == 10:
            d, txt = self.verify_request()

        # verify response
        elif t == 11:
            d, txt = self.verify_response() 

        # test request
        else:
            d, txt = 0xffffffff, 'TODO...' 
        
        s0 = self.bytepos[4][0]
        s1 = self.bytepos[dlen+3][1]
        # show data in hex
        self.putx(s0, s1, [3, ['H:%08x' % d, 'DATA']])
        # show data in comment
        self.putx(s0, s1, [8, [txt, txt]])
        # show data in full text string
        self.text += ' - ' + txt
                
    def decode_pkt(self):
	# Packet header
        self.head = self.datapkt[0:4]
        
        self.putx(self.bytepos[0][0], self.bytepos[1][1],
                  [2, ['HEAD:%04x' % ((self.head[0] << 8) | self.head[1]), 'HD']])
        self.puthead()
        
	# Cmd
        self.putx(self.bytepos[2][0], self.bytepos[2][1],
                  [2, ['CMD:%02x' % (self.head[2]), 'CMD']])

	# Data length
        if self.data_len():
            self.putx(self.bytepos[3][0], self.bytepos[3][1], 
                      [2, ['LEN:%02x' % (self.head[3]), 'LEN']])
	
        # Decode data payload
        self.decode_data_msg()

        # CRC check 
        self.crc = self.datapkt[self.plen-1]
        ccrc = self.compute_crc8()
        if  self.crc != ccrc:
            self.putwarn('Bad CRC %02x != %02x' % (self.crc, ccrc), 'CRC!')
    
        self.putx(self.ss_block, self.es_block,
                      [4, ['CRC:%02x' % (self.crc), 'CRC']])
        
	# Full text trace
        if self.options['fulltext'] == 'yes':
            self.putx(self.bytepos[0][0], self.es_block, [9, [self.text, '...']])

    def decode(self, ss, es, data):
        ptype, rxtx, pdata = data
        self.ss_block, self.es_block = ss, es

        # just keep DATA
        if ptype != 'DATA':
            return

        # '0xAA' is the SOP of a ufcs package
        if pdata[0] == 0xaa:
            self.putx(ss, es, [3, ['SOP:0xaa', 'SOP']])
            self.reset()
            return

        # append data to packet
        self.datapkt.append(pdata[0])
        # append byte pos (start byte, end byte )
        self.bytepos.append((ss, es))
        			
    	# ctrl_msg or data_msg?
        if self.dataidx == 3 :
            if (self.datapkt[1] & 1) == 1:
                self.plen = self.datapkt[3] + 5
            else:
                self.plen = 4

        self.dataidx += 1

        # packet is full
        if self.dataidx == self.plen:
            self.decode_pkt()

