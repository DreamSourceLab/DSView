##
## This file is part of the sigrok project.
##
## Copyright (C) 2015 Uwe Hermann <uwe@hermann-uwe.de>
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

# Normal commands (CMD)
# Unlisted items are 'Reserved' as per SD spec. The 'Unknown' items don't
# seem to be mentioned in the spec, but aren't marked as reserved either.
cmd_names = {
    0:  'GO_IDLE_STATE',
    # 1: Reserved
    2:  'ALL_SEND_CID',
    3:  'SEND_RELATIVE_ADDR',
    4:  'SET_DSR',
    5:  'IO_SEND_OP_COND', # SDIO-only
    6:  'SWITCH_FUNC', # New since spec 1.10
    7:  'SELECT/DESELECT_CARD',
    8:  'SEND_IF_COND',
    9:  'SEND_CSD',
    10: 'SEND_CID',
    11: 'VOLTAGE_SWITCH',
    12: 'STOP_TRANSMISSION',
    13: 'SEND_STATUS',
    # 14: Reserved
    15: 'GO_INACTIVE_STATE',
    16: 'SET_BLOCKLEN',
    17: 'READ_SINGLE_BLOCK',
    18: 'READ_MULTIPLE_BLOCK',
    19: 'SEND_TUNING_BLOCK',
    20: 'SPEED_CLASS_CONTROL',
    # 21-22: Reserved
    23: 'SET_BLOCK_COUNT',
    24: 'WRITE_BLOCK',
    25: 'WRITE_MULTIPLE_BLOCK',
    26: 'Reserved for manufacturer',
    27: 'PROGRAM_CSD',
    28: 'SET_WRITE_PROT',
    29: 'CLR_WRITE_PROT',
    30: 'SEND_WRITE_PROT',
    # 31: Reserved
    32: 'ERASE_WR_BLK_START',
    33: 'ERASE_WR_BLK_END',
    34: 'Reserved for CMD6', # New since spec 1.10
    35: 'Reserved for CMD6', # New since spec 1.10
    36: 'Reserved for CMD6', # New since spec 1.10
    37: 'Reserved for CMD6', # New since spec 1.10
    38: 'ERASE',
    # 39: Reserved
    40: 'Reserved for security specification',
    # 41: Reserved
    42: 'LOCK_UNLOCK',
    # 43-49: Reserved
    50: 'Reserved for CMD6', # New since spec 1.10
    # 51: Reserved
    52: 'IO_RW_DIRECT', # SDIO-only
    53: 'IO_RW_EXTENDED', # SDIO-only
    54: 'Unknown',
    55: 'APP_CMD',
    56: 'GEN_CMD',
    57: 'Reserved for CMD6', # New since spec 1.10
    # 58-59: Reserved
    60: 'Reserved for manufacturer',
    61: 'Reserved for manufacturer',
    62: 'Reserved for manufacturer',
    63: 'Reserved for manufacturer',
}

# Application-specific commands (ACMD)
# Unlisted items are 'Reserved' as per SD spec. The 'Unknown' items don't
# seem to be mentioned in the spec, but aren't marked as reserved either.
acmd_names = {
    # 1-5: Reserved
    6:  'SET_BUS_WIDTH',
    # 7-12: Reserved
    13: 'SD_STATUS',
    14: 'Reserved for Security Application',
    15: 'Reserved for Security Application',
    16: 'Reserved for Security Application',
    # 17: Reserved
    18: 'Reserved for SD security applications',
    # 19-21: Reserved
    22: 'SEND_NUM_WR_BLOCKS',
    23: 'SET_WR_BLK_ERASE_COUNT',
    # 24: Reserved
    25: 'Reserved for SD security applications',
    26: 'Reserved for SD security applications',
    27: 'Reserved for security specification',
    28: 'Reserved for security specification',
    # 29: Reserved
    30: 'Reserved for security specification',
    31: 'Reserved for security specification',
    32: 'Reserved for security specification',
    33: 'Reserved for security specification',
    34: 'Reserved for security specification',
    35: 'Reserved for security specification',
    # 36-37: Reserved
    38: 'Reserved for SD security applications',
    # 39-40: Reserved
    41: 'SD_SEND_OP_COND',
    42: 'SET_CLR_CARD_DETECT',
    43: 'Reserved for SD security applications',
    44: 'Reserved for SD security applications',
    45: 'Reserved for SD security applications',
    46: 'Reserved for SD security applications',
    47: 'Reserved for SD security applications',
    48: 'Reserved for SD security applications',
    49: 'Reserved for SD security applications',
    50: 'Unknown',
    51: 'SEND_SCR',
    52: 'Reserved for security specification',
    53: 'Reserved for security specification',
    54: 'Reserved for security specification',
    55: 'Non-existant', # Doesn't exist (equivalent to CMD55)
    56: 'Reserved for security specification',
    57: 'Reserved for security specification',
    58: 'Reserved for security specification',
    59: 'Reserved for security specification',
    60: 'Unknown',
    61: 'Unknown',
    62: 'Unknown',
    63: 'Unknown',
}

accepted_voltages = {
    0b0001: '2.7-3.6V',
    0b0010: 'reserved for low voltage range',
    0b0100: 'reserved',
    0b1000: 'reserved',
    # All other values: "not defined".
}

card_status = {
    0:  'Reserved for manufacturer test mode',
    1:  'Reserved for manufacturer test mode',
    2:  'Reserved for application specific commands',
    3:  'AKE_SEQ_ERROR',
    4:  'Reserved for SDIO card',
    5:  'APP_CMD',
    6:  'Unknown',
    7:  'Unknown',
    8:  'READY_FOR_DATA',
    9:  'CURRENT_STATE', # CURRENT_STATE is a 4-bit value (decimal: 0..15).
    10: 'CURRENT_STATE',
    11: 'CURRENT_STATE',
    12: 'CURRENT_STATE',
    13: 'ERASE_RESET',
    14: 'CARD_ECC_DISABLED',
    15: 'WP_ERASE_SKIP',
    16: 'CSD_OVERWRITE',
    17: 'Reserved for DEFERRED_RESPONSE', # See eSD addendum
    18: 'Reserved',
    19: 'ERROR',
    20: 'CC_ERROR',
    21: 'CARD_ECC_FAILED',
    22: 'ILLEGAL_COMMAND',
    23: 'COM_CRC_ERROR',
    24: 'LOCK_UNLOCK_FAILED',
    25: 'CARD_IS_LOCKED',
    26: 'WP_VIOLATION',
    27: 'ERASE_PARAM',
    28: 'ERASE_SEQ_ERROR',
    29: 'BLOCK_LEN_ERROR',
    30: 'ADDRESS_ERROR',
    31: 'OUT_OF_RANGE',
}

sd_status = {
    # 311:0: Reserved for manufacturer
    # 391:312: Reserved
}
