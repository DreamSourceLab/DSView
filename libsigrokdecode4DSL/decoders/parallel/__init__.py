##
## This file is part of the libsigrokdecode project.
##
## Copyright (C) 2013 Uwe Hermann <uwe@hermann-uwe.de>
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

'''
This protocol decoder can decode synchronous parallel buses with various
number of data bits/channels and one (optional) clock line.

If no clock line is supplied, the decoder works slightly differently in
that it interprets every transition on any of the supplied data channels
like there had been a clock transition.

It is required to use the lowest data channels, and use consecutive ones.
For example, for a 4-bit sync parallel bus, channels D0/D1/D2/D3 (and CLK)
should be used. Using combinations like D7/D12/D3/D15 is not supported.
For an 8-bit bus you should use D0-D7, for a 16-bit bus use D0-D15 and so on.
'''

from .pd import Decoder
