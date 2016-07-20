##
## This file is part of the libsigrokdecode project.
##
## Copyright (C) 2014 Uwe Hermann <uwe@hermann-uwe.de>
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

# Addresses/devices. Items that are not listed are reserved/unknown.
address = {
    0x40: 'Matsui TV',
}

digits = {
    0: ['0', '0'],
    1: ['1', '1'],
    2: ['2', '2'],
    3: ['3', '3'],
    4: ['4', '4'],
    5: ['5', '5'],
    6: ['6', '6'],
    7: ['7', '7'],
    8: ['8', '8'],
    9: ['9', '9'],
}

# Commands. Items that are not listed are reserved/unknown.
command = {
    0x40: dict(list(digits.items()) + list({
        11: ['-/--', '-/--'],
        16: ['Mute', 'M'],
        18: ['Standby', 'StBy'],
        26: ['Volume up', 'Vol+'],
        27: ['Program up', 'P+'],
        30: ['Volume down', 'Vol-'],
        31: ['Program down', 'P-'],
        68: ['AV', 'AV'],
    }.items())),
}
