##
## This file is part of the libsigrokdecode project.
##
## Copyright (C) 2013 Bert Vermeulen <bert@biot.com>
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

MODULE_ID = {
    0x01: 'GBIC',
    0x02: 'Integrated module/connector',
    0x03: 'SFP',
    0x04: '300-pin XBI',
    0x05: 'XENPAK',
    0x06: 'XFP',
    0x07: 'XFF',
    0x08: 'XFP-E',
    0x09: 'XPAK',
    0x0a: 'X2',
}

ALARM_THRESHOLDS = {
    0:  "Temp high alarm",
    2:  "Temp low alarm",
    4:  "Temp high warning",
    6:  "Temp low warning",
    16: "Bias high alarm",
    18: "Bias low alarm",
    20: "Bias high warning",
    22: "Bias low warning",
    24: "TX power high alarm",
    26: "TX power low alarm",
    28: "TX power high warning",
    30: "TX power low warning",
    32: "RX power high alarm",
    34: "RX power low alarm",
    36: "RX power high warning",
    38: "RX power low warning",
    40: "AUX 1 high alarm",
    42: "AUX 1 low alarm",
    44: "AUX 1 high warning",
    46: "AUX 1 low warning",
    48: "AUX 2 high alarm",
    50: "AUX 2 low alarm",
    52: "AUX 2 high warning",
    54: "AUX 2 low warning",
}

AD_READOUTS = {
    0:  "Module temperature",
    4:  "TX bias current",
    6:  "Measured TX output power",
    8:  "Measured RX input power",
    10: "AUX 1 measurement",
    12: "AUX 2 measurement",
}

GCS_BITS = [
    "TX disable",
    "Soft TX disable",
    "MOD_NR",
    "P_Down",
    "Soft P_Down",
    "Interrupt",
    "RX_LOS",
    "Data_Not_Ready",
    "TX_NR",
    "TX_Fault",
    "TX_CDR not locked",
    "RX_NR",
    "RX_CDR not locked",
]

CONNECTOR = {
    0x01:   "SC",
    0x02:   "Fibre Channel style 1 copper",
    0x03:   "Fibre Channel style 2 copper",
    0x04:   "BNC/TNC",
    0x05:   "Fibre Channel coax",
    0x06:   "FiberJack",
    0x07:   "LC",
    0x08:   "MT-RJ",
    0x09:   "MU",
    0x0a:   "SG",
    0x0b:   "Optical pigtail",
    0x20:   "HSSDC II",
    0x21:   "Copper pigtail",
}

TRANSCEIVER = [
    # 10GB Ethernet
    ["10GBASE-SR", "10GBASE-LR", "10GBASE-ER", "10GBASE-LRM", "10GBASE-SW",
        "10GBASE-LW",   "10GBASE-EW"],
    # 10GB Fibre Channel
    ["1200-MX-SN-I", "1200-SM-LL-L", "Extended Reach 1550 nm",
        "Intermediate reach 1300 nm FP"],
    # 10GB Copper
    [],
    # 10GB low speed
    ["1000BASE-SX / 1xFC MMF", "1000BASE-LX / 1xFC SMF", "2xFC MMF",
        "2xFC SMF", "OC48-SR", "OC48-IR", "OC48-LR"],
    # 10GB SONET/SDH interconnect
    ["I-64.1r", "I-64.1", "I-64.2r", "I-64.2", "I-64.3", "I-64.5"],
    # 10GB SONET/SDH short haul
    ["S-64.1", "S-64.2a", "S-64.2b", "S-64.3a", "S-64.3b", "S-64.5a", "S-64.5b"],
    # 10GB SONET/SDH long haul
    ["L-64.1", "L-64.2a", "L-64.2b", "L-64.2c", "L-64.3", "G.959.1 P1L1-2D2"],
    # 10GB SONET/SDH very long haul
    ["V-64.2a", "V-64.2b", "V-64.3"],
]

SERIAL_ENCODING = [
    "64B/66B",
    "8B/10B",
    "SONET scrambled",
    "NRZ",
    "RZ",
]

XMIT_TECH = [
    "850 nm VCSEL",
    "1310 nm VCSEL",
    "1550 nm VCSEL",
    "1310 nm FP",
    "1310 nm DFB",
    "1550 nm DFB",
    "1310 nm EML"
    "1550 nm EML"
    "copper",
]

CDR = [
    "9.95Gb/s",
    "10.3Gb/s",
    "10.5Gb/s",
    "10.7Gb/s",
    "11.1Gb/s",
    "(unknown)",
    "lineside loopback mode",
    "XFI loopback mode",
]

DEVICE_TECH = [
    ["no wavelength control", "sctive wavelength control"],
    ["uncooled transmitter device", "cooled transmitter"],
    ["PIN detector", "APD detector"],
    ["transmitter not tunable", "transmitter tunable"],
]

ENHANCED_OPTS = [
    "VPS",
    "soft TX_DISABLE",
    "soft P_Down",
    "VPS LV regulator mode",
    "VPS bypassed regulator mode",
    "active FEC control",
    "wavelength tunability",
    "CMU",
]

AUX_TYPES = [
    "not implemented",
    "APD bias voltage",
    "(unknown)",
    "TEC current",
    "laser temperature",
    "laser wavelength",
    "5V supply voltage",
    "3.3V supply voltage",
    "1.8V supply voltage",
    "-5.2V supply voltage",
    "5V supply current",
    "(unknown)",
    "(unknown)",
    "3.3V supply current",
    "1.8V supply current",
    "-5.2V supply current",
]

class Decoder(srd.Decoder):
    api_version = 2
    id = 'xfp'
    name = 'XFP'
    longname = '10 Gigabit Small Form Factor Pluggable Module (XFP)'
    desc = 'Data structure describing display device capabilities.'
    license = 'gplv3+'
    inputs = ['i2c']
    outputs = ['xfp']
    annotations = (
        ('fieldnames-and-values', 'XFP structure field names and values'),
        ('fields', 'XFP structure fields'),
    )

    def __init__(self, **kwargs):
        # Received data items, used as an index into samplenum/data
        self.cnt = -1
        # Start/end sample numbers per data item
        self.sn = []
        # Multi-byte structure buffer
        self.buf = []
        # Filled in by address 0x7f in low memory
        self.cur_highmem_page = 0
        # Filled in by extended ID value in table 2
        self.have_clei = False
        # Handlers for each field in the structure, keyed by the end
        # index of that field. Each handler is fed all unhandled bytes
        # up until that point, so mark unused space with the dummy
        # handler self.ignore().
        self.MAP_LOWER_MEMORY = {
            0:  self.module_id,
            1:  self.signal_cc,
            57: self.alarm_warnings,
            59: self.vps,
            69: self.ignore,
            71: self.ber,
            75: self.wavelength_cr,
            79: self.fec_cr,
            95: self.int_ctrl,
            109: self.ad_readout,
            111: self.gcs,
            117: self.ignore,
            118: self.ignore,
            122: self.ignore,
            126: self.ignore,
            127: self.page_select,
        }
        self.MAP_HIGH_TABLE_1 = {
            128: self.module_id,
            129: self.ext_module_id,
            130: self.connector,
            138: self.transceiver,
            139: self.serial_encoding,
            140: self.br_min,
            141: self.br_max,
            142: self.link_length_smf,
            143: self.link_length_e50,
            144: self.link_length_50um,
            145: self.link_length_625um,
            146: self.link_length_copper,
            147: self.device_tech,
            163: self.vendor,
            164: self.cdr,
            167: self.vendor_oui,
            183: self.vendor_pn,
            185: self.vendor_rev,
            187: self.wavelength,
            189: self.wavelength_tolerance,
            190: self.max_case_temp,
            191: self.ignore,
            195: self.power_supply,
            211: self.vendor_sn,
            219: self.manuf_date,
            220: self.diag_mon,
            221: self.enhanced_opts,
            222: self.aux_mon,
            223: self.ignore,
            255: self.maybe_ascii,
        }

    def start(self):
        self.out_ann = self.register(srd.OUTPUT_ANN)

    def decode(self, ss, es, data):
        cmd, data = data

        # We only care about actual data bytes that are read (for now).
        if cmd != 'DATA READ':
            return

        self.cnt += 1
        self.sn.append([ss, es])

        self.buf.append(data)
        if self.cnt < 0x80:
            if self.cnt in self.MAP_LOWER_MEMORY:
                self.MAP_LOWER_MEMORY[self.cnt](self.buf)
                self.buf.clear()
        elif self.cnt < 0x0100 and self.cur_highmem_page == 0x01:
            # Serial ID memory map
            if self.cnt in self.MAP_HIGH_TABLE_1:
                self.MAP_HIGH_TABLE_1[self.cnt](self.buf)
                self.buf.clear()

    # Annotation helper
    def annotate(self, key, value, start_cnt=None, end_cnt=None):
        if start_cnt is None:
            start_cnt = self.cnt - len(self.buf) + 1
        if end_cnt is None:
            end_cnt = self.cnt
        self.put(self.sn[start_cnt][0], self.sn[end_cnt][1],
                self.out_ann, [0, [key + ": " + value]])
        self.put(self.sn[start_cnt][0], self.sn[end_cnt][1],
                 self.out_ann, [1, [value]])

    # Placeholder handler, needed to advance the buffer past unused or
    # reserved space in the structures.
    def ignore(self, data):
        pass

    # Show as ASCII if possible
    def maybe_ascii(self, data):
        for i in range(len(data)):
            if data[i] >= 0x20 and data[i] < 0x7f:
                cnt = self.cnt - len(data) + 1
                self.annotate("Vendor ID", chr(data[i]), cnt, cnt)

    # Convert 16-bit two's complement values, with each increment
    # representing 1/256C, to degrees Celsius.
    def to_temp(self, value):
        if value & 0x8000:
            value = -((value ^ 0xffff) + 1)
        temp = value / 256.0
        return "%.1f C" % temp

    # TX bias current in uA. Each increment represents 0.2uA
    def to_current(self, value):
        current = value / 500000.0
        return "%.1f mA" % current

    # Power in mW, with each increment representing 0.1uW
    def to_power(self, value):
        power = value / 10000.0
        return "%.2f mW" % power

    # Wavelength in increments of 0.05nm
    def to_wavelength(self, value):
        wl = value / 20
        return "%d nm" % wl

    # Wavelength in increments of 0.005nm
    def to_wavelength_tolerance(self, value):
        wl = value / 200.0
        return "%.1f nm" % wl

    def module_id(self, data):
        self.annotate("Module identifier", MODULE_ID.get(data[0], "Unknown"))

    def signal_cc(self, data):
        # No good data available.
        if (data[0] != 0x00):
            self.annotate("Signal Conditioner Control", "%.2x" % data[0])

    def alarm_warnings(self, data):
        cnt_idx = self.cnt - len(data)
        idx = 0
        while idx < 56:
            if idx == 8:
                # Skip over reserved A/D flag thresholds
                idx += 8
            value = (data[idx] << 8) | data[idx + 1]
            if value != 0:
                name = ALARM_THRESHOLDS.get(idx, "...")
                if idx in (0, 2, 4, 6):
                    self.annotate(name, self.to_temp(value),
                            cnt_idx + idx, cnt_idx + idx + 1)
                elif idx in (16, 18, 20, 22):
                    self.annotate(name, self.to_current(value),
                            cnt_idx + idx, cnt_idx + idx + 1)
                elif idx in (24, 26, 28, 30, 32, 34, 36, 38):
                    self.annotate(name, self.to_power(value),
                            cnt_idx + idx, cnt_idx + idx + 1)
                else:
                    self.annotate(name, "%d" % name, value, cnt_idx + idx,
                            cnt_idx + idx + 1)
            idx += 2

    def vps(self, data):
        # No good data available.
        if (data != [0, 0]):
            self.annotate("VPS", "%.2x%.2x" % (data[0], data[1]))

    def ber(self, data):
        # No good data available.
        if (data != [0, 0]):
            self.annotate("BER", str(data))

    def wavelength_cr(self, data):
        # No good data available.
        if (data != [0, 0, 0, 0]):
            self.annotate("WCR", str(data))

    def fec_cr(self, data):
        if (data != [0, 0, 0, 0]):
            self.annotate("FEC", str(data))

    def int_ctrl(self, data):
        # No good data available. Also boring.
        out = []
        for d in data:
            out.append("%.2x" % d)
        self.annotate("Interrupt bits", ' '.join(out))

    def ad_readout(self, data):
        cnt_idx = self.cnt - len(data) + 1
        idx = 0
        while idx < 14:
            if idx == 2:
                # Skip over reserved field
                idx += 2
            value = (data[idx] << 8) | data[idx + 1]
            name = AD_READOUTS.get(idx, "...")
            if value != 0:
                if idx == 0:
                    self.annotate(name, self.to_temp(value),
                            cnt_idx + idx, cnt_idx + idx + 1)
                elif idx == 4:
                    self.annotate(name, self.to_current(value),
                            cnt_idx + idx, cnt_idx + idx + 1)
                elif idx in (6, 8):
                    self.annotate(name, self.to_power(value),
                            cnt_idx + idx, cnt_idx + idx + 1)
                else:
                    self.annotate(name, str(value), cnt_idx + idx,
                            cnt_idx + idx + 1)
            idx += 2

    def gcs(self, data):
        allbits = (data[0] << 8) | data[1]
        out = []
        for b in range(13):
            if allbits & 0x8000:
                out.append(GCS_BITS[b])
            allbits <<= 1
        self.annotate("General Control/Status", ', '.join(out))

    def page_select(self, data):
        self.cur_highmem_page = data[0]

    def ext_module_id(self, data):
        out = ["Power level %d module" % ((data[0] >> 6) + 1)]
        if data[0] & 0x20 == 0:
            out.append("CDR")
        if data[0] & 0x10 == 0:
            out.append("TX ref clock input required")
        if data[0] & 0x08 == 0:
            self.have_clei = True
        self.annotate("Extended id", ', '.join(out))

    def connector(self, data):
        if data[0] in CONNECTOR:
            self.annotate("Connector", CONNECTOR[data[0]])

    def transceiver(self, data):
        out = []
        for t in range(8):
            if data[t] == 0:
                continue
            value = data[t]
            for b in range(8):
                if value & 0x80:
                    if len(TRANSCEIVER[t]) < b + 1:
                        out.append("(unknown)")
                    else:
                        out.append(TRANSCEIVER[t][b])
                value <<= 1
        self.annotate("Transceiver compliance", ', '.join(out))

    def serial_encoding(self, data):
        out = []
        value = data[0]
        for b in range(8):
            if value & 0x80:
                if len(SERIAL_ENCODING) < b + 1:
                    out.append("(unknown)")
                else:
                    out.append(SERIAL_ENCODING[b])
                value <<= 1
        self.annotate("Serial encoding support", ', '.join(out))

    def br_min(self, data):
        # Increments represent 100Mb/s
        rate = data[0] / 10.0
        self.annotate("Minimum bit rate", "%.3f GB/s" % rate)

    def br_max(self, data):
        # Increments represent 100Mb/s
        rate = data[0] / 10.0
        self.annotate("Maximum bit rate", "%.3f GB/s" % rate)

    def link_length_smf(self, data):
        if data[0] == 0:
            length = "(standard)"
        elif data[0] == 255:
            length = "> 254 km"
        else:
            length = "%d km" % data[0]
        self.annotate("Link length (SMF)", length)

    def link_length_e50(self, data):
        if data[0] == 0:
            length = "(standard)"
        elif data[0] == 255:
            length = "> 508 m"
        else:
            length = "%d m" % (data[0] * 2)
        self.annotate("Link length (extended, 50μm MMF)", length)

    def link_length_50um(self, data):
        if data[0] == 0:
            length = "(standard)"
        elif data[0] == 255:
            length = "> 254 m"
        else:
            length = "%d m" % data[0]
        self.annotate("Link length (50μm MMF)", length)

    def link_length_625um(self, data):
        if data[0] == 0:
            length = "(standard)"
        elif data[0] == 255:
            length = "> 254 m"
        else:
            length = "%d m" % (data[0])
        self.annotate("Link length (62.5μm MMF)", length)

    def link_length_copper(self, data):
        if data[0] == 0:
            length = "(unknown)"
        elif data[0] == 255:
            length = "> 254 m"
        else:
            length = "%d m" % (data[0] * 2)
        self.annotate("Link length (copper)", length)

    def device_tech(self, data):
        out = []
        xmit = data[0] >> 4
        if xmit <= len(XMIT_TECH) - 1:
            out.append("%s transmitter" % XMIT_TECH[xmit])
        dev = data[0] & 0x0f
        for b in range(4):
            out.append(DEVICE_TECH[b][(dev >> (3 - b)) & 0x01])
        self.annotate("Device technology", ', '.join(out))

    def vendor(self, data):
        name = bytes(data).strip().decode('ascii').strip('\x00')
        if name:
            self.annotate("Vendor", name)

    def cdr(self, data):
        out = []
        value = data[0]
        for b in range(8):
            if value & 0x80:
                out.append(CDR[b])
            value <<= 1
        self.annotate("CDR support", ', '.join(out))

    def vendor_oui(self, data):
        if data != [0, 0, 0]:
            self.annotate("Vendor OUI", "%.2X-%.2X-%.2X" % tuple(data))

    def vendor_pn(self, data):
        name = bytes(data).strip().decode('ascii').strip('\x00')
        if name:
            self.annotate("Vendor part number", name)

    def vendor_rev(self, data):
        name = bytes(data).strip().decode('ascii').strip('\x00')
        if name:
            self.annotate("Vendor revision", name)

    def wavelength(self, data):
        value = (data[0] << 8) | data[1]
        self.annotate("Wavelength", self.to_wavelength(value))

    def wavelength_tolerance(self, data):
        value = (data[0] << 8) | data[1]
        self.annotate("Wavelength tolerance", self.to_wavelength_tolerance(value))

    def max_case_temp(self, data):
        self.annotate("Maximum case temperature", "%d C" % data[0])

    def power_supply(self, data):
        out = []
        self.annotate("Max power dissipation",
                "%.3f W" % (data[0] * 0.02), self.cnt - 3, self.cnt - 3)
        self.annotate("Max power dissipation (powered down)",
                "%.3f W" % (data[1] * 0.01), self.cnt - 2, self.cnt - 2)
        value = (data[2] >> 4) * 0.050
        self.annotate("Max current required (5V supply)",
                "%.3f A" % value, self.cnt - 1, self.cnt - 1)
        value = (data[2] & 0x0f) * 0.100
        self.annotate("Max current required (3.3V supply)",
                "%.3f A" % value, self.cnt - 1, self.cnt - 1)
        value = (data[3] >> 4) * 0.100
        self.annotate("Max current required (1.8V supply)",
                "%.3f A" % value, self.cnt, self.cnt)
        value = (data[3] & 0x0f) * 0.050
        self.annotate("Max current required (-5.2V supply)",
                "%.3f A" % value, self.cnt, self.cnt)

    def vendor_sn(self, data):
        name = bytes(data).strip().decode('ascii').strip('\x00')
        if name:
            self.annotate("Vendor serial number", name)

    def manuf_date(self, data):
        y = int(bytes(data[0:2])) + 2000
        m = int(bytes(data[2:4]))
        d = int(bytes(data[4:6]))
        mnf = "%.4d-%.2d-%.2d" % (y, m, d)
        lot = bytes(data[6:]).strip().decode('ascii').strip('\x00')
        if lot:
            mnf += " lot " + lot
        self.annotate("Manufacturing date", mnf)

    def diag_mon(self, data):
        out = []
        if data[0] & 0x10:
            out.append("BER support")
        else:
            out.append("no BER support")
        if data[0] & 0x08:
            out.append("average power measurement")
        else:
            out.append("OMA power measurement")
        self.annotate("Diagnostic monitoring", ', '.join(out))

    def enhanced_opts(self, data):
        out = []
        value = data[0]
        for b in range(8):
            if value & 0x80:
                out.append(ENHANCED_OPTS[b])
            value <<= 1
        self.annotate("Enhanced option support", ', '.join(out))

    def aux_mon(self, data):
        aux = AUX_TYPES[data[0] >> 4]
        self.annotate("AUX1 monitoring", aux)
        aux = AUX_TYPES[data[0] & 0x0f]
        self.annotate("AUX2 monitoring", aux)
