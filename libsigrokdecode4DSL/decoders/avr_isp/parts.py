##
## This file is part of the libsigrokdecode project.
##
## Copyright (C) 2012 Uwe Hermann <uwe@hermann-uwe.de>
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

# Device code addresses:
# 0x00: vendor code, 0x01: part family + flash size, 0x02: part number

# Vendor code
vendor_code = {
    0x1e: 'Atmel',
    0x00: 'Device locked',
}

# (Part family + flash size, part number)
part = {
    (0x90, 0x01): 'AT90S1200',
    (0x91, 0x01): 'AT90S2313',
    (0x92, 0x01): 'AT90S4414',
    (0x92, 0x05): 'ATmega48', # 4kB flash
    (0x93, 0x01): 'AT90S8515',
    (0x93, 0x0a): 'ATmega88', # 8kB flash
    (0x94, 0x06): 'ATmega168', # 16kB flash
    (0xff, 0xff): 'Device code erased, or target missing',
    (0x01, 0x02): 'Device locked',
    # TODO: Lots more entries.
}
