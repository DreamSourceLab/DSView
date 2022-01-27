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

import sigrokdecode as srd

# ...
fields = {
    # START field (indicates the start of a transaction)
    'START': {
        0b1: 'Start of FSI cycle',
    },
}

class Decoder(srd.Decoder):
    api_version = 3
    id = 'fsi'
    name = 'FSI'
    longname = 'Flexible Service Interface'
    desc = 'Protocol for FSI devices on Raptor OpenPOWER systems.'
    license = 'agplv3'
    inputs = ['logic']
    outputs = []
    tags = ['PC']
    channels = (
        {'id': 'data',  'name': 'DATA',  'desc': 'Frame'},
        {'id': 'clock', 'name': 'CLOCK', 'desc': 'Clock'},
    )
    annotations = (
        ('warnings', 'Warnings'),
        ('start', 'Start'),
        ('cycle-type', 'Cycle type'),
        ('direction', 'Direction'),
        ('addr', 'Address'),
        ('data', 'Data'),
        ('commands', 'Commands'),
        ('crc', 'CRC'),
        ('turn-around', 'TAR'),
    )
    annotation_rows = (
        ('data', 'Data', (1, 2, 3, 4, 5, 6, 7, 8,)),
        ('warnings', 'Warnings', (0,)),
    )

    def __init__(self):
        self.tar_cycles = 3
        self.reset()

    def reset(self):
        self.state = 'IDLE'
        self.break_start_sample_number = 0
        self.break_counter = 0
        self.samplenum = 0
        self.samplenum_prev = 0
        self.fsi_data_prev = 0
        self.fsi_data_break_prev = 0
        self.crc_internal = 0
        self.response_received = 0
        self.crc_calculating = 0
        self.busy_seq_count = 0
        self.valid_response = 0
        self.prev_address = {}
        self.prev_address_valid = {0: False, 1: False, 2: False, 3: False}
        self.ss_block = None
        self.es_block = None

    def start(self):
        self.out_ann = self.register(srd.OUTPUT_ANN)

    def putb(self, data):
        self.put(self.ss_block, self.es_block, self.out_ann, data)

    def decode(self):
        while True:
            # FSI is clocked out on the falling edge and latched in on the rising edge of the clock...according to the specification.
            # That said, the specification is not clear on what this actually means (and it doesn't follow industry convention).
            # Real IBM POWER9 hardware is verified to work in the following mode:
            # FSI data being sent to the master is latched by the master logic at the falling edge of the FSI clock
            # FSI data being from to the master is strobed by the master logic at the falling edge of the FSI clock
            # FSI data being sent to the slave is latched by the slave logic at the rising edge of the FSI clock
            # FSI data being from to the slave is strobed by the slave logic at the rising edge of the FSI clock
            #
            # The above means that we have to be able to make a reasonable guess as to who is transmitting (the master or the slave)
            # in order to know which edge to sample on

            # Wait for either clock edge
            (data, clk) = self.wait({1: 'e'})

            # FSI data is electrically inverted
            fsi_data = not data
            fsi_clk = clk
            current_sample_number = self.samplenum

            # Detect BREAK commands
            # Note these can only be sent by the master, so sample on the rising clock edge only
            if (fsi_clk):
                if (self.fsi_data_break_prev == 1):
                    self.break_counter = self.break_counter + 1
                    if (self.break_counter == 256):
                        self.ss_block = self.break_start_sample_number
                        self.es_block = current_sample_number
                        self.putb([6, ['BREAK']])
                        self.state = 'BREAK_TAR_QUEUED'
                        self.busy_seq_count = 0
                        self.valid_response = 0
                        self.prev_address = {}
                        self.prev_address_valid = {0: False, 1: False, 2: False, 3: False}
                        self.ss_block = current_sample_number
                else:
                    if (self.break_counter > 256):
                        self.es_block = current_sample_number
                        self.putb([0, ['BREAK asserted in excess of specification cycles']])
                    self.break_start_sample_number = current_sample_number
                    self.break_counter = 0
                self.fsi_data_break_prev = fsi_data

            if ((self.state == 'TAR') or (self.state == 'RX_SLAVE_ID') or (self.state == 'RESPONSE') or (self.state == 'RX_DATA')
                or (self.state == 'RX_IPOLL_INTERRUPT_FIELD') or (self.state == 'RX_IPOLL_DMA_CONTROL_FIELD') or (self.state == 'RX_IPOLL_DMA_CONTROL_FIELD')
                or ((self.state == 'CRC') and (self.valid_response))):
                # Slave is / should be transmitting, sample on falling clock edge only
                if (fsi_clk):
                    continue
            else:
                # Master is / should be transmitting, sample on rising clock edge only
                if (not fsi_clk):
                    continue

            # Transfer state machine
            if (self.state == 'IDLE'):
                self.crc_internal = 0
                self.response_received = 0
                if (self.fsi_data_prev == 1):
                    self.tx_slave_id = 0
                    self.data_count = 2
                    self.ss_block = self.samplenum_prev
                    self.es_block = current_sample_number
                    self.putb([1, ['START']])
                    self.ss_block = current_sample_number
                    self.crc_calculating = 1
                    self.state = 'TX_SLAVE_ID'

            elif (self.state == 'TX_SLAVE_ID'):
                self.crc_calculating = 1
                if (self.data_count > 0):
                    self.tx_slave_id = (self.tx_slave_id >> 1) | (self.fsi_data_prev << 1)
                    self.data_count = self.data_count - 1
                    if (self.data_count == 0):
                        self.es_block = current_sample_number
                        self.putb([5, ['Slave ID: 0x%01x' % self.tx_slave_id]])
                        self.ss_block = current_sample_number
                        self.command_count = 0
                        self.command_code = 0
                        self.command = None
                        self.valid_command = False
                        self.state = 'COMMAND'

            elif (self.state == 'COMMAND'):
                self.crc_calculating = 1
                self.command_code = (self.command_code << 1) | self.fsi_data_prev
                self.command_count = self.command_count + 1
                if ((self.command_count == 3) and (self.command_code == 0b100)):
                    self.command = 'ABS_ADR'
                    self.valid_command = True
                elif ((self.command_count == 3) and (self.command_code == 0b101)):
                    self.command = 'REL_ADR'
                    self.valid_command = True
                elif ((self.command_count == 2) and (self.command_code == 0b11)):
                    self.command = 'SAME_ADR'
                    self.valid_command = True
                elif ((self.command_count == 3) and (self.command_code == 0b010)):
                    self.command = 'D_POLL'
                    self.valid_command = True
                elif ((self.command_count == 3) and (self.command_code == 0b011)):
                    self.command = 'E_POLL'
                    self.valid_command = True
                elif ((self.command_count == 3) and (self.command_code == 0b001)):
                    self.command = 'I_POLL'
                    self.valid_command = True
                if ((self.command_count > 7) or (self.valid_command == True)):
                    if (self.command_count == 8):
                        self.es_block = current_sample_number
                        self.putb([6, ['Invalid command code: 0x%02x/%d' % (self.command_code, self.command_count)]])
                        self.putb([0, ['%s' % 'Invalid command code']])
                        self.ss_block = current_sample_number
                        self.state = 'IDLE'
                    else:
                        self.es_block = current_sample_number
                        self.putb([6, ['Command: %s (0x%02x/%d)' % (self.command, self.command_code, self.command_count)]])
                        self.ss_block = current_sample_number
                        if (self.command == 'ABS_ADR'):
                            self.address_length = 21
                            self.address_count = 0
                            self.address = 0
                            self.state = 'DIRECTION'
                        elif (self.command == 'REL_ADR'):
                            self.address_length = 8
                            self.address_count = 0
                            self.address = 0
                            self.state = 'DIRECTION'
                        elif (self.command == 'SAME_ADR'):
                            self.address_length = 2
                            self.address_count = 0
                            self.address = 0
                            self.state = 'DIRECTION'
                        elif (self.command == 'D_POLL'):
                            self.crc = 0
                            self.crc_count = 0
                            self.state = 'CRC'
                        elif (self.command == 'E_POLL'):
                            self.crc = 0
                            self.crc_count = 0
                            self.state = 'CRC'
                        elif (self.command == 'I_POLL'):
                            self.crc = 0
                            self.crc_count = 0
                            self.state = 'CRC'
                        else:
                            self.state = 'IDLE'

            elif (self.state == 'DIRECTION'):
                self.crc_calculating = 1
                self.direction = self.fsi_data_prev
                self.es_block = current_sample_number
                if (self.direction == 1):
                    self.putb([3, ['Direction: %s' % 'Read']])
                else:
                    self.putb([3, ['Direction: %s' % 'Write']])
                self.ss_block = current_sample_number
                if (self.command == 'REL_ADR'):
                    self.state = 'REL_ADDRESS_SIGN'
                else:
                    self.state = 'ADDRESS'

            elif (self.state == 'REL_ADDRESS_SIGN'):
                self.crc_calculating = 1
                self.relative_address_negative = self.fsi_data_prev
                self.es_block = current_sample_number
                if (self.relative_address_negative == 1):
                    self.putb([5, ['Relative address sign: %s' % '(-)']])
                else:
                    self.putb([5, ['Relative address sign: %s' % '(+)']])
                self.ss_block = current_sample_number
                self.state = 'ADDRESS'

            elif (self.state == 'ADDRESS'):
                self.crc_calculating = 1
                self.address = (self.address << 1) | self.fsi_data_prev
                self.address_count = self.address_count + 1
                if (self.address_count >= self.address_length):
                    self.address_raw = self.address
                    if (self.prev_address_valid[self.tx_slave_id]):
                        if (self.command == 'SAME_ADR'):
                            self.address = (self.prev_address[self.tx_slave_id] & ~0b11) | (self.address_raw & 0b11)
                        elif (self.command == 'REL_ADR'):
                            if (self.relative_address_negative):
                                self.address = self.prev_address[self.tx_slave_id] - (0x100 - self.address_raw)
                            else:
                                self.address = self.prev_address[self.tx_slave_id] + self.address_raw
                    self.es_block = current_sample_number
                    if (((self.command == 'SAME_ADR') or (self.command == 'REL_ADR'))):
                        self.putb([5, ['Address: 0x%06x (0x%03x)' % (self.address, self.address_raw)]])
                        if (not self.prev_address_valid[self.tx_slave_id]):
                            self.putb([0, ['%s' % 'Base address for relative address not captured']])
                    else:
                        self.putb([5, ['Address: 0x%06x' % self.address]])
                    self.ss_block = current_sample_number
                    self.state = 'DATA_SIZE'

            elif (self.state == 'DATA_SIZE'):
                self.crc_calculating = 1
                if (self.direction and ((self.address_raw & 3) == 3) and self.fsi_data_prev):
                    # OpenFSI suffers from an unfortunate conflict between the SAME_ADR command
                    # and the TERM command.  Both start with 2'b11 and since both are variable
                    # length it is impossible to determine if a TERM command was sent until this
                    # point in the receiver process!
                    #
                    # Set correct command code for further processing
                    self.command_code = 0b111111
                    self.command_count = 6
                    self.command = 'TERM'
                    self.es_block = current_sample_number
                    self.putb([6, ['Command: %s (0x%02x/%d)' % (self.command, self.command_code, self.command_count)]])
                    self.ss_block = current_sample_number
                    self.direction = 0
                    self.busy_seq_count = 0
                    self.crc = 0
                    self.crc_count = 0
                    self.state = 'CRC'
                else:
                    if (self.fsi_data_prev == 0):
                        self.data_size = 'BYTE'
                    else:
                        if ((self.address_raw & 3) == 1):
                            self.data_size = 'WORD'
                            # Force lowest address bits to specification-mandated values if required
                            self.address = (self.address & ~3) | 1
                        elif ((self.address_raw & 1) == 0):
                            self.data_size = 'HALF_WORD'
                            # Force lowest address bits to specification-mandated values if required
                            self.address = self.address & ~1
                        else:
                            self.data_size = 'UNKNOWN'
                    self.es_block = current_sample_number
                    if (self.data_size == 'UNKNOWN'):
                        self.putb([0, ['Data Size: %s' % 'UNKNOWN']])
                        self.state = 'IDLE'
                    else:
                        self.putb([3, ['Data Size: %s' % self.data_size]])
                        if (self.direction == 1):
                            self.crc = 0
                            self.crc_count = 0
                            self.state = 'CRC'
                        else:
                            self.data = 0
                            self.data_count = 0
                            if (self.data_size == 'BYTE'):
                                self.data_length = 8
                            elif (self.data_size == 'HALF_WORD'):
                                self.data_length = 16
                            elif (self.data_size == 'WORD'):
                                self.data_length = 32
                            else:
                                self.data_size = None
                            self.state = 'TX_DATA'
                self.ss_block = current_sample_number

            elif (self.state == 'TX_DATA'):
                self.crc_calculating = 1
                self.data = (self.data << 1) | self.fsi_data_prev
                self.data_count = self.data_count + 1
                if (self.data_count >= self.data_length):
                    self.es_block = current_sample_number
                    if (self.data_size == 'BYTE'):
                        self.putb([5, ['Data: 0x%02x' % self.data]])
                    elif (self.data_size == 'HALF_WORD'):
                        self.putb([5, ['Data: 0x%04x' % self.data]])
                    else:
                        self.putb([5, ['Data: 0x%08x' % self.data]])
                    self.ss_block = current_sample_number
                    self.crc = 0
                    self.crc_count = 0
                    self.state = 'CRC'

            elif (self.state == 'CRC'):
                if (self.crc_count == 0):
                    self.computed_crc_tx_end = self.crc_internal
                self.crc_calculating = 1
                self.crc = (self.crc << 1) | self.fsi_data_prev
                self.crc_count = self.crc_count + 1
                if (self.crc_count >= 4):
                    self.es_block = current_sample_number
                    if (self.crc == self.computed_crc_tx_end):
                        self.putb([7, ['CRC: 0x%01x (GOOD)' % self.crc]])
                        if (self.response_received):
                            if (((self.command == 'ABS_ADR') or (self.command == 'REL_ADR') or (self.command == 'SAME_ADR'))
                                and ((self.response == 'ACK_D') or (self.response == 'ACK'))):
                                self.prev_address[self.tx_slave_id] = self.address
                                self.prev_address_valid[self.tx_slave_id] = True
                    else:
                        self.putb([7, ['CRC: 0x%01x (BAD)' % self.crc]])
                        self.putb([0, ['%s' % 'Bad CRC']])
                    self.ss_block = current_sample_number
                    self.tar_timer = 0
                    self.state = 'TAR'
                    self.timeout_counter = 0

            elif (self.state == 'BREAK_TAR_QUEUED'):
                # Special case, since break operates outside of the main state machine
                # This state is a safe entry point into the main state machine
                self.tar_timer = 0
                self.state = 'BREAK_TAR'

            elif (self.state == 'BREAK_TAR'):
                self.tar_timer = self.tar_timer + 1
                if (self.tar_timer > self.tar_cycles):
                    self.crc_calculating = 0
                    self.crc_internal = 0
                    self.es_block = current_sample_number
                    self.putb([8, ['%s' % 'TAR']])
                    self.ss_block = current_sample_number
                    self.state = 'IDLE'

            elif (self.state == 'TAR'):
                self.crc_calculating = 0
                self.crc_internal = 0
                self.tar_timer = self.tar_timer + 1
                if (self.tar_timer > self.tar_cycles):
                    if (self.response_received == 1):
                        self.response_received = 0
                        if (self.rx_slave_id == self.tx_slave_id):
                            if (self.response == 'BUSY'):
                                self.busy_seq_count = self.busy_seq_count + 1
                            else:
                                self.busy_seq_count = 0
                            # Sequence complete
                            self.state = 'IDLE'
                    if (self.timeout_counter == 0):
                        self.es_block = self.samplenum_prev
                        self.putb([8, ['%s' % 'TAR']])
                        self.ss_block = current_sample_number
                    if (self.fsi_data_prev == 1):
                        self.crc_calculating = 1
                        self.rx_slave_id = 0
                        self.data_count = 2
                        if (self.state == 'IDLE'):
                            # Already processed response message, was going to IDLE state
                            self.state = 'TX_SLAVE_ID'
                        else:
                            self.state = 'RX_SLAVE_ID'
                        self.ss_block = self.samplenum_prev
                        self.es_block = current_sample_number
                        self.putb([1, ['START']])
                        self.ss_block = current_sample_number
                    else:
                        self.timeout_counter = self.timeout_counter + 1
                        if (self.timeout_counter >= 256):
                            self.es_block = current_sample_number
                            self.putb([8, ['%s' % 'Response timeout']])
                            self.putb([0, ['%s' % 'Response timeout']])
                            self.state = 'IDLE'

            elif (self.state == 'RX_SLAVE_ID'):
                self.crc_calculating = 1
                self.response_received = 1
                if (self.data_count > 0):
                    self.rx_slave_id = (self.rx_slave_id >> 1) | (self.fsi_data_prev << 1)
                    self.data_count = self.data_count - 1
                    if (self.data_count == 0):
                        self.es_block = current_sample_number
                        self.putb([5, ['Slave ID: 0x%01x' % self.rx_slave_id]])
                        if (self.rx_slave_id != self.tx_slave_id):
                            self.putb([0, ['%s' % 'Slave ID does not match active transaction']])
                        self.ss_block = current_sample_number
                        self.response_count = 0
                        self.response_code = 0
                        self.response = None
                        self.valid_response = False
                        self.state = 'RESPONSE'

            elif (self.state == 'RESPONSE'):
                self.crc_calculating = 1
                self.response_code = (self.response_code << 1) | self.fsi_data_prev
                self.response_count = self.response_count + 1
                if ((self.command == 'I_POLL') and (self.rx_slave_id == self.tx_slave_id) and (self.response_count == 1) and (self.response_code == 0b0)):
                    self.response = 'I_POLL_RSP'
                    self.valid_response = True
                elif ((self.response_count == 2) and (self.response_code == 0b00)):
                    if (self.direction == 1):
                        self.response = 'ACK_D'
                    else:
                        self.response = 'ACK'
                    self.valid_response = True
                elif ((self.response_count == 2) and (self.response_code == 0b01)):
                    self.response = 'BUSY'
                    self.valid_response = True
                elif ((self.response_count == 2) and (self.response_code == 0b10)):
                    self.response = 'ERR_A'
                    self.valid_response = True
                elif ((self.response_count == 2) and (self.response_code == 0b11)):
                    self.response = 'ERR_C'
                    self.valid_response = True
                if ((self.response_count > 2) or (self.valid_response == True)):
                    if (self.response_count == 8):
                        self.es_block = current_sample_number
                        self.putb([6, ['Invalid response code: 0x%02x/%d' % (self.response_code, self.response_count)]])
                        self.putb([0, ['%s' % 'Invalid response code']])
                        self.ss_block = current_sample_number
                        self.state = 'IDLE'
                    else:
                        self.es_block = current_sample_number
                        self.putb([6, ['Response: %s (0x%02x/%d)' % (self.response, self.response_code, self.response_count)]])
                        self.ss_block = current_sample_number
                        if (self.response == 'ACK_D'):
                            self.data = 0
                            self.data_count = 0
                            if (self.data_size == 'BYTE'):
                                self.data_length = 8
                            elif (self.data_size == 'HALF_WORD'):
                                self.data_length = 16
                            elif (self.data_size == 'WORD'):
                                self.data_length = 32
                            else:
                                self.data_size = None
                            #self.state = 'DIRECTION'
                            self.state = 'RX_DATA'
                        elif (self.response == 'ACK'):
                            self.crc = 0
                            self.crc_count = 0
                            self.state = 'CRC'
                        elif (self.response == 'BUSY'):
                            self.crc = 0
                            self.crc_count = 0
                            self.state = 'CRC'
                        elif (self.response == 'ERR_A'):
                            self.crc = 0
                            self.crc_count = 0
                            self.state = 'CRC'
                        elif (self.response == 'ERR_C'):
                            self.crc = 0
                            self.crc_count = 0
                            self.state = 'CRC'
                        elif (self.response == 'I_POLL_RSP'):
                            self.data = 0
                            self.data_count = 0
                            self.data_length = 2
                            self.state = 'RX_IPOLL_INTERRUPT_FIELD'
                        else:
                            self.state = 'IDLE'

            elif (self.state == 'RX_DATA'):
                self.crc_calculating = 1
                self.data = (self.data << 1) | self.fsi_data_prev
                self.data_count = self.data_count + 1
                if (self.data_count >= self.data_length):
                    self.es_block = current_sample_number
                    self.putb([5, ['Data: 0x%08x' % self.data]])
                    self.ss_block = current_sample_number
                    self.crc = 0
                    self.crc_count = 0
                    self.state = 'CRC'

            elif (self.state == 'RX_IPOLL_INTERRUPT_FIELD'):
                self.crc_calculating = 1
                self.data = (self.data << 1) | self.fsi_data_prev
                self.data_count = self.data_count + 1
                if (self.data_count >= self.data_length):
                    self.es_block = current_sample_number
                    self.putb([5, ['Interrupt Field: 0x%01x' % self.data]])
                    self.ss_block = current_sample_number
                    self.data = 0
                    self.data_count = 0
                    self.data_length = 3
                    self.state = 'RX_IPOLL_DMA_CONTROL_FIELD'

            elif (self.state == 'RX_IPOLL_DMA_CONTROL_FIELD'):
                self.crc_calculating = 1
                self.data = (self.data << 1) | self.fsi_data_prev
                self.data_count = self.data_count + 1
                if (self.data_count >= self.data_length):
                    self.es_block = current_sample_number
                    self.putb([5, ['DMA Control Field: 0x%01x' % self.data]])
                    self.ss_block = current_sample_number
                    self.crc = 0
                    self.crc_count = 0
                    self.state = 'CRC'

            # CRC calculation
            # Implement Galios-type LFSR for polynomial 0x7 (MSB first)
            crc_prev = self.crc_internal
            if (self.crc_calculating):
                crc_feedback = (((crc_prev >> 3) & 1) ^ self.fsi_data_prev) & 1
            if (self.crc_calculating):
                self.crc_internal = (self.crc_internal & ~(1 << 0)) | ((crc_feedback & 1) << 0)
                self.crc_internal = (self.crc_internal & ~(1 << 1)) | ((((crc_prev & 1) ^ crc_feedback) & 1) << 1)
                self.crc_internal = (self.crc_internal & ~(1 << 2)) | (((((crc_prev >> 1) & 1) ^ crc_feedback) & 1) << 2)
                self.crc_internal = (self.crc_internal & ~(1 << 3)) | ((((crc_prev >> 2) & 1) & 1) << 3)

            self.fsi_data_prev = fsi_data
            self.samplenum_prev = current_sample_number
