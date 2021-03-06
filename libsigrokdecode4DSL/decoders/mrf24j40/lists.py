##
## This file is part of the libsigrokdecode project.
##
## Copyright (C) 2015 Karl Palsson <karlp@tweak.net.au>
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

sregs = {
    0: 'RXMCR',
    1: 'PANIDL',
    2: 'PANIDH',
    3: 'SADRL',
    4: 'SADRH',
    5: 'EADR0',
    6: 'EADR1',
    7: 'EADR2',
    8: 'EADR3',
    9: 'EADR4',
    0xa: 'EADR5',
    0xb: 'EADR6',
    0xc: 'EADR7',
    0xd: 'RXFLUSH',
    0xe: 'Reserved',
    0xf: 'Reserved',
    0x10: 'ORDER',
    0x11: 'TXMCR',
    0x12: 'ACKTMOUT',
    0x13: 'ESLOTG1',
    0x14: 'SYMTICKL',
    0x15: 'SYMTICKH',
    0x16: 'PACON0',
    0x17: 'PACON1',
    0x18: 'PACON2',
    0x19: 'Reserved',
    0x1a: 'TXBCON0',
    0x1b: 'TXNCON',
    0x1c: 'TXG1CON',
    0x1d: 'TXG2CON',
    0x1e: 'ESLOTG23',
    0x1f: 'ESLOTG45',
    0x20: 'ESLOTG67',
    0x21: 'TXPEND',
    0x22: 'WAKECON',
    0x23: 'FRMOFFSET',
    0x24: 'TXSTAT',
    0x25: 'TXBCON1',
    0x26: 'GATECLK',
    0x27: 'TXTIME',
    0x28: 'HSYMTIMRL',
    0x29: 'HSYMTIMRH',
    0x2a: 'SOFTRST',
    0x2b: 'Reserved',
    0x2c: 'SECCON0',
    0x2d: 'SECCON1',
    0x2e: 'TXSTBL',
    0x2f: 'Reserved',
    0x30: 'RXSR',
    0x31: 'INTSTAT',
    0x32: 'INTCON',
    0x33: 'GPIO',
    0x34: 'TRISGPIO',
    0x35: 'SLPACK',
    0x36: 'RFCTL',
    0x37: 'SECCR2',
    0x38: 'BBREG0',
    0x39: 'BBREG1',
    0x3a: 'BBREG2',
    0x3b: 'BBREG3',
    0x3c: 'BBREG4',
    0x3d: 'Reserved',
    0x3e: 'BBREG6',
    0x3f: 'CCAEDTH',
}

lregs = {
    0x200: 'RFCON0',
    0x201: 'RFCON1',
    0x202: 'RFCON2',
    0x203: 'RFCON3',
    0x204: 'Reserved',
    0x205: 'RFCON5',
    0x206: 'RFCON6',
    0x207: 'RFCON7',
    0x208: 'RFCON8',
    0x209: 'SLPCAL0',
    0x20A: 'SLPCAL1',
    0x20B: 'SLPCAL2',
    0x20C: 'Reserved',
    0x20D: 'Reserved',
    0x20E: 'Reserved',
    0x20F: 'RFSTATE',
    0x210: 'RSSI',
    0x211: 'SLPCON0',
    0x212: 'Reserved',
    0x213: 'Reserved',
    0x214: 'Reserved',
    0x215: 'Reserved',
    0x216: 'Reserved',
    0x217: 'Reserved',
    0x218: 'Reserved',
    0x219: 'Reserved',
    0x21A: 'Reserved',
    0x21B: 'Reserved',
    0x21C: 'Reserved',
    0x21D: 'Reserved',
    0x21E: 'Reserved',
    0x21F: 'Reserved',
    0x220: 'SLPCON1',
    0x221: 'Reserved',
    0x222: 'WAKETIMEL',
    0x223: 'WAKETIMEH',
    0x224: 'REMCNTL',
    0x225: 'REMCNTH',
    0x226: 'MAINCNT0',
    0x227: 'MAINCNT1',
    0x228: 'MAINCNT2',
    0x229: 'MAINCNT3',
    0x22A: 'Reserved',
    0x22B: 'Reserved',
    0x22C: 'Reserved',
    0x22D: 'Reserved',
    0x22E: 'Reserved',
    0x22F: 'TESTMODE',
    0x230: 'ASSOEADR0',
    0x231: 'ASSOEADR1',
    0x232: 'ASSOEADR2',
    0x233: 'ASSOEADR3',
    0x234: 'ASSOEADR4',
    0x235: 'ASSOEADR5',
    0x236: 'ASSOEADR6',
    0x237: 'ASSOEADR7',
    0x238: 'ASSOSADR0',
    0x239: 'ASSOSADR1',
    0x23A: 'Reserved',
    0x23B: 'Reserved',
    0x23C: 'Unimplemented',
    0x23D: 'Unimplemented',
    0x23E: 'Unimplemented',
    0x23F: 'Unimplemented',
    0x240: 'UPNONCE0',
    0x241: 'UPNONCE1',
    0x242: 'UPNONCE2',
    0x243: 'UPNONCE3',
    0x244: 'UPNONCE4',
    0x245: 'UPNONCE5',
    0x246: 'UPNONCE6',
    0x247: 'UPNONCE7',
    0x248: 'UPNONCE8',
    0x249: 'UPNONCE9',
    0x24A: 'UPNONCE10',
    0x24B: 'UPNONCE11',
    0x24C: 'UPNONCE12'
}
