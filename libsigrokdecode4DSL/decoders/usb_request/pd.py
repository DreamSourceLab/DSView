##
## This file is part of the libsigrokdecode project.
##
## Copyright (C) 2015 Stefan Brüns <stefan.bruens@rwth-aachen.de>
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

class SamplerateError(Exception):
    pass

class pcap_usb_pkt():
    # Linux usbmon format, see Documentation/usb/usbmon.txt
    h  = b'\x00\x00\x00\x00' # ID part 1
    h += b'\x00\x00\x00\x00' # ID part 2
    h += b'C'                # 'S'ubmit / 'C'omplete / 'E'rror
    h += b'\x03'             # ISO (0), Intr, Control, Bulk (3)
    h += b'\x00'             # Endpoint
    h += b'\x00'             # Device address
    h += b'\x00\x00'         # Bus number
    h += b'-'                # Setup tag - 0: Setup present, '-' otherwise
    h += b'<'                # Data tag - '<' no data, 0 otherwise
    # Timestamp
    h += b'\x00\x00\x00\x00' # TS seconds part 1
    h += b'\x00\x00\x00\x00' # TS seconds part 2
    h += b'\x00\x00\x00\x00' # TS useconds
    #
    h += b'\x00\x00\x00\x00' # Status 0: OK
    h += b'\x00\x00\x00\x00' # URB length
    h += b'\x00\x00\x00\x00' # Data length
    # Setup packet data, valid if setup tag == 0
    h += b'\x00'             # bmRequestType
    h += b'\x00'             # bRequest
    h += b'\x00\x00'         # wValue
    h += b'\x00\x00'         # wIndex
    h += b'\x00\x00'         # wLength
    #
    h += b'\x00\x00\x00\x00' # ISO/interrupt interval
    h += b'\x00\x00\x00\x00' # ISO start frame
    h += b'\x00\x00\x00\x00' # URB flags
    h += b'\x00\x00\x00\x00' # Number of ISO descriptors

    def __init__(self, req, ts, is_submit):
        self.header = bytearray(pcap_usb_pkt.h)
        self.data = b''
        self.set_urbid(req['id'])
        self.set_urbtype('S' if is_submit else 'C')
        self.set_timestamp(ts)
        self.set_addr_ep(req['addr'], req['ep'])
        if req['type'] in ('SETUP IN', 'SETUP OUT'):
            self.set_transfertype(2) # Control
            self.set_setup(req['setup_data'])
        if req['type'] in ('BULK IN'):
            self.set_addr_ep(req['addr'], 0x80 | req['ep'])
        self.set_data(req['data'])

    def set_urbid(self, urbid):
        self.header[4:8] = struct.pack('>I', urbid)

    def set_urbtype(self, urbtype):
        self.header[8] = ord(urbtype)

    def set_transfertype(self, transfertype):
        self.header[9] = transfertype

    def set_addr_ep(self, addr, ep):
        self.header[11] = addr
        self.header[10] = ep

    def set_timestamp(self, ts):
        self.timestamp = ts
        self.header[20:24] = struct.pack('>I', ts[0]) # seconds
        self.header[24:28] = struct.pack('>I', ts[1]) # microseconds

    def set_data(self, data):
        self.data = data
        self.header[15] = 0
        self.header[36:40] = struct.pack('>I', len(data))

    def set_setup(self, data):
        self.header[14] = 0
        self.header[40:48] = data

    def packet(self):
        return bytes(self.header) + bytes(self.data)

    def record_header(self):
        # See https://wiki.wireshark.org/Development/LibpcapFileFormat.
        (secs, usecs) = self.timestamp
        h  = struct.pack('>I', secs) # TS seconds
        h += struct.pack('>I', usecs) # TS microseconds
        # No truncation, so both lengths are the same.
        h += struct.pack('>I', len(self)) # Captured len (usb hdr + data)
        h += struct.pack('>I', len(self)) # Original len
        return h

    def __len__(self):
        return 64 + len(self.data)

class Decoder(srd.Decoder):
    api_version = 2
    id = 'usb_request'
    name = 'USB request'
    longname = 'Universal Serial Bus (LS/FS) transaction/request'
    desc = 'USB (low-speed and full-speed) transaction/request protocol.'
    license = 'gplv2+'
    inputs = ['usb_packet']
    outputs = ['usb_request']
    annotations = (
        ('request-setup-read', 'Setup: Device-to-host'),
        ('request-setup-write', 'Setup: Host-to-device'),
        ('request-bulk-read', 'Bulk: Device-to-host'),
        ('request-bulk-write', 'Bulk: Host-to-device'),
        ('errors', 'Unexpected packets'),
    )
    annotation_rows = (
        ('request', 'USB requests', tuple(range(4))),
        ('errors', 'Errors', (4,)),
    )
    binary = (
        ('pcap', 'PCAP format'),
    )

    def __init__(self):
        self.samplerate = None
        self.request = {}
        self.request_id = 0
        self.transaction_state = 'IDLE'
        self.ss_transaction = None
        self.es_transaction = None
        self.transaction_ep = None
        self.transaction_addr = None
        self.wrote_pcap_header = False

    def putr(self, ss, es, data):
        self.put(ss, es, self.out_ann, data)

    def putb(self, ts, data):
        self.put(ts, ts, self.out_binary, data)

    def pcap_global_header(self):
        # See https://wiki.wireshark.org/Development/LibpcapFileFormat.
        h  = b'\xa1\xb2\xc3\xd4' # Magic, indicate microsecond ts resolution
        h += b'\x00\x02'         # Major version 2
        h += b'\x00\x04'         # Minor version 4
        h += b'\x00\x00\x00\x00' # Correction vs. UTC, seconds
        h += b'\x00\x00\x00\x00' # Timestamp accuracy
        h += b'\xff\xff\xff\xff' # Max packet len
        # LINKTYPE_USB_LINUX_MMAPPED 220
        # Linux usbmon format, see Documentation/usb/usbmon.txt.
        h += b'\x00\x00\x00\xdc' # Link layer
        return h

    def metadata(self, key, value):
        if key == srd.SRD_CONF_SAMPLERATE:
            self.samplerate = value
            self.secs_per_sample = float(1) / float(self.samplerate)

    def start(self):
        self.out_binary = self.register(srd.OUTPUT_BINARY)
        self.out_ann = self.register(srd.OUTPUT_ANN)

    def handle_transfer(self):
        request_started = 0
        request_end = self.handshake in ('ACK', 'STALL', 'timeout')
        ep = self.transaction_ep
        addr = self.transaction_addr
        if not (addr, ep) in self.request:
            self.request[(addr, ep)] = {'setup_data': [], 'data': [],
                'type': None, 'ss': self.ss_transaction, 'es': None,
                'id': self.request_id, 'addr': addr, 'ep': ep}
            self.request_id += 1
            request_started = 1
        request = self.request[(addr,ep)]

        # BULK or INTERRUPT transfer
        if request['type'] in (None, 'BULK IN') and self.transaction_type == 'IN':
            request['type'] = 'BULK IN'
            request['data'] += self.transaction_data
            request['es'] = self.es_transaction
            self.handle_request(request_started, request_end)
        elif request['type'] in (None, 'BULK OUT') and self.transaction_type == 'OUT':
            request['type'] = 'BULK OUT'
            request['data'] += self.transaction_data
            request['es'] = self.es_transaction
            self.handle_request(request_started, request_end)

        # CONTROL, SETUP stage
        elif request['type'] is None and self.transaction_type == 'SETUP':
            request['setup_data'] = self.transaction_data
            request['wLength'] = struct.unpack('<H',
                bytes(self.transaction_data[6:8]))[0]
            if self.transaction_data[0] & 0x80:
                request['type'] = 'SETUP IN'
                self.handle_request(1, 0)
            else:
                request['type'] = 'SETUP OUT'
                self.handle_request(request['wLength'] == 0, 0)

        # CONTROL, DATA stage
        elif request['type'] == 'SETUP IN' and self.transaction_type == 'IN':
            request['data'] += self.transaction_data

        elif request['type'] == 'SETUP OUT' and self.transaction_type == 'OUT':
            request['data'] += self.transaction_data
            if request['wLength'] == len(request['data']):
                self.handle_request(1, 0)

        # CONTROL, STATUS stage
        elif request['type'] == 'SETUP IN' and self.transaction_type == 'OUT':
            request['es'] = self.es_transaction
            self.handle_request(0, request_end)

        elif request['type'] == 'SETUP OUT' and self.transaction_type == 'IN':
            request['es'] = self.es_transaction
            self.handle_request(0, request_end)

        else:
            return

        return

    def ts_from_samplenum(self, sample):
        ts = float(sample) * self.secs_per_sample
        return (int(ts), int((ts % 1.0) * 1e6))

    def write_pcap_header(self):
        if not self.wrote_pcap_header:
            #self.put(0, 0, self.out_binary, [0, self.pcap_global_header()])
            self.wrote_pcap_header = True

    def request_summary(self, request):
        s = '['
        if request['type'] in ('SETUP IN', 'SETUP OUT'):
            for b in request['setup_data']:
                s += ' %02X' % b
            s += ' ]['
        for b in request['data']:
            s += ' %02X' % b
        s += ' ] : %s' % self.handshake
        return s

    def handle_request(self, request_start, request_end):
        if request_start != 1 and request_end != 1:
            return
        self.write_pcap_header()
        ep = self.transaction_ep
        addr = self.transaction_addr
        request = self.request[(addr, ep)]

        ss, es = request['ss'], request['es']

        if request_start == 1:
            # Issue PCAP 'SUBMIT' packet.
            ts = self.ts_from_samplenum(ss)
            pkt = pcap_usb_pkt(request, ts, True)
            #self.putb(ss, [0, pkt.record_header()])
            #self.putb(ss, [0, pkt.packet()])

        if request_end == 1:
            # Write annotation.
            summary = self.request_summary(request)
            if request['type'] == 'SETUP IN':
                self.putr(ss, es, [0, ['SETUP in: %s' % summary]])
            elif request['type'] == 'SETUP OUT':
                self.putr(ss, es, [1, ['SETUP out: %s' % summary]])
            elif request['type'] == 'BULK IN':
                self.putr(ss, es, [2, ['BULK in: %s' % summary]])
            elif request['type'] == 'BULK OUT':
                self.putr(ss, es, [3, ['BULK out: %s' % summary]])

            # Issue PCAP 'COMPLETE' packet.
            ts = self.ts_from_samplenum(es)
            pkt = pcap_usb_pkt(request, ts, False)
            #self.putb(ss, [0, pkt.record_header()])
            #self.putb(ss, [0, pkt.packet()])
            del self.request[(addr, ep)]

    def decode(self, ss, es, data):
        if not self.samplerate:
            raise SamplerateError('Cannot decode without samplerate.')
        ptype, pdata = data

        # We only care about certain packet types for now.
        if ptype not in ('PACKET'):
            return

        pcategory, pname, pinfo = pdata

        if pcategory == 'TOKEN':
            if pname == 'SOF':
                return
            if self.transaction_state == 'TOKEN RECEIVED':
                transaction_timeout = self.es_transaction
                # Token length is 35 bits, timeout is 16..18 bit times
                # (USB 2.0 7.1.19.1).
                transaction_timeout += int((self.es_transaction - self.ss_transaction) / 2)
                if ss > transaction_timeout:
                    self.es_transaction = transaction_timeout
                    self.handshake = 'timeout'
                    self.handle_transfer()
                    self.transaction_state = 'IDLE'

            if self.transaction_state != 'IDLE':
                self.putr(ss, es, [4, ['ERR: received %s token in state %s' %
                    (pname, self.transaction_state)]])
                return

            sync, pid, addr, ep, crc5 = pinfo
            self.transaction_data = []
            self.ss_transaction = ss
            self.es_transaction = es
            self.transaction_state = 'TOKEN RECEIVED'
            self.transaction_ep = ep
            self.transaction_addr = addr
            self.transaction_type = pname # IN OUT SETUP

        elif pcategory == 'DATA':
            if self.transaction_state != 'TOKEN RECEIVED':
                self.putr(ss, es, [4, ['ERR: received %s token in state %s' %
                    (pname, self.transaction_state)]])
                return

            self.transaction_data = pinfo[2]
            self.transaction_state = 'DATA RECEIVED'

        elif pcategory == 'HANDSHAKE':
            if self.transaction_state not in ('TOKEN RECEIVED', 'DATA RECEIVED'):
                self.putr(ss, es, [4, ['ERR: received %s token in state %s' %
                    (pname, self.transaction_state)]])
                return

            self.handshake = pname
            self.transaction_state = 'IDLE'
            self.es_transaction = es
            self.handle_transfer()

        elif pname == 'PRE':
            return

        else:
            self.putr(ss, es, [4, ['ERR: received unhandled %s token in state %s' %
                (pname, self.transaction_state)]])
            return
