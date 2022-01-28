##
## This file is part of the libsigrokdecode project.
##
## Copyright (C) 2020 Raptor Engineering, LLC <support@raptorengineering.com>
##
## This program is free software: you can redistribute it and/or modify
## it under the terms of the GNU Affero General Public License as
## published by the Free Software Foundation, either version 3 of the
## License, or (at your option) any later version.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU Affero General Public License for more details.
##
## You should have received a copy of the GNU Affero General Public License
## along with this program.  If not, see <https://www.gnu.org/licenses/>.
##

'''
FSI is a low level serial protocol used by various devices on OpenPOWER
systems such as the Raptor Talos II and Blackbird.
'''

from .pd import Decoder
