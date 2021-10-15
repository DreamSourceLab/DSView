##
## This file is part of the libsigrokdecode project.
##
## Copyright (C) 2021 ghecko - Jordan Ovrè <ghecko78@gmail.com>
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
import re
from collections import deque
from .lists import fifo_registers


class Ann:
    """ Annotation ID """
    READ, WRITE, ADDRESS, DATA, VMK = range(5)


class Operation:
    """ TPM transaction type """
    READ = 0x80
    WRITE = 0x00


class TransactionState:
    """ State of the transaction """
    READ = 0
    WRITE = 1
    READ_ADDRESS = 2
    WAIT = 3
    TRANSFER_DATA = 4


class Transaction:
    """Represent one TPM SPI transaction
    Args:
        start_sample: The absolute samplenumber of the first sample of this transaction.
        operation: Transaction type.
        transfer_size: The number of data bytes.
    Attributes:
        start_sample (int): The absolute samplenumber of the first sample of this transaction.
        end_sample (int): The absolute samplenumber of the last sample of this transaction.
        operation (Operation): Transaction type.
        address (bytearray): The register address in the transaction. (big-endian).
        data (bytearray): Data in the transaction.
        size (int): The number of data bytes.
        wait_count (int): Holds the number of wait states between the address and data .
    """

    def __init__(self, start_sample, operation, transfer_size):
        self.start_sample = start_sample
        self.end_sample_op = None
        self.end_sample_addr = None
        self.end_sample_data = None
        self.operation = operation
        self.address = bytearray()
        self.data = bytearray()
        self.size = transfer_size
        self.wait_count = 0

    def is_complete(self):
        """
        Check if the transaction is complete.
        A transaction is complete when all address and data bytes are captured.
        """
        return self.is_address_complete() and self.is_data_complete()

    def is_data_complete(self):
        """ Check if all data bytes are captured """
        return len(self.data) == self.size

    def is_address_complete(self):
        """ Check if all address bytes are captured. """
        return len(self.address) == 3

    def frame(self):
        """ Return address and data annotation if the transaction is complete. """
        if self.is_complete():
            register_name = ""
            try:
                register_name = fifo_registers[int.from_bytes(self.address, "big") & 0xffff]
            except KeyError:
                register_name = "Unknown"
            wait_str = '' if self.wait_count == 0 else ' (Waits: {})'.format(self.wait_count)
            data_str = ''.join('{:02x}'.format(x) for x in self.data)
            op_ann = ['Read', 'Rd'] if self.operation == Operation.READ else ['Write', 'Wr']
            addr_ann = ['Register: {}'.format(register_name), '{}'.format(register_name)]
            data_ann = ['{}{}'.format(data_str, wait_str), '{}'.format(data_str), data_str]
            return ((self.start_sample, self.end_sample_op, op_ann),
                    (self.end_sample_op, self.end_sample_addr, addr_ann),
                    (self.end_sample_addr, self.end_sample_data, data_ann))
        return None


class Decoder(srd.Decoder):
    api_version = 3
    id = 'spi_tpm'
    name = 'SPI TPM'
    longname = 'SPI TPM transactions'
    desc = 'Parses TPM transactions from SPI bus with automatic VMK extraction for BitLocker.'
    license = 'gplv2+'
    inputs = ['spi']
    outputs = []
    tags = ['IC', 'TPM', 'BitLocker']
    annotations = (
        ('Read', 'Read register operation'),
        ('Write', 'Write register operation'),
        ('Address', 'Register address'),
        ('Data', 'Data'),
        ('VMK', 'Extracted BitLocker VMK'),
    )
    annotation_rows = (
        ('Transactions', 'TPM transactions', (0, 1, 2, 3)),
        ('B-VMK', 'BitLocker Volume Master Key', (4,)),
    )
    options = (
        {'id': 'wait_mask', 'desc': 'TPM Wait transfer Mask', 'default': '0x00',
         'values': ('0x00', '0xFE')},
    )

    def __init__(self):
        self.end_wait = 0x01
        self.operation_mask = 0x80
        self.address_mask = 0x3f
        # Circular buffer to detect VMK header on transaction data
        self.queue = deque(maxlen=12)
        self.vmk_meta = {"s_queue": deque(maxlen=12), "vmk_ss": 0, "vmk_es": 0}
        self.saving_vmk = False
        self.vmk = []
        self.reset()
        self.state_machine = None
        self.init_state_machine()

    def reset(self):
        self.ss = self.es = 0
        self.state = None
        self.current_transaction = None

    def end_current_transaction(self):
        self.reset()

    def start(self):
        self.out_ann = self.register(srd.OUTPUT_ANN)

    def init_state_machine(self):
        self.state_machine = {
            TransactionState.READ: self._transaction_read,
            TransactionState.WRITE: self._transaction_write,
            TransactionState.READ_ADDRESS: self._transaction_read_addr,
            TransactionState.WAIT: self._transaction_wait,
            TransactionState.TRANSFER_DATA: self._transaction_data,
            None: self._return
        }

    def _return(self, mosi, miso):
        return

    def transaction_state(self, mosi):
        if (mosi & self.operation_mask) == Operation.READ:
            return TransactionState.READ
        elif (mosi & self.operation_mask) == Operation.WRITE:
            return TransactionState.WRITE
        else:
            return None

    def _transaction_wait(self, mosi, miso):
        self.current_transaction.wait_count += 1
        if miso == self.end_wait:
            self.state = TransactionState.TRANSFER_DATA

    def _transaction_read(self, mosi, miso):
        # TPM operation is defined on the 7th bit of the first byte of the transaction (1=read / 0=write)
        # transfer size is defined on bits 0 to 5 of the first byte of the transaction
        transfer_size = (mosi & 0x3f) + 1
        self.current_transaction = Transaction(self.ss, Operation.READ, transfer_size)
        self.current_transaction.end_sample_op = self.es
        self.state = TransactionState.READ_ADDRESS

    def _transaction_write(self, mosi, miso):
        # TPM operation is defined on the 7th bit of the first byte of the transaction (1=read / 0=write)
        # transfer size is defined on bits 0 to 5 of the first byte of the transaction
        transfer_size = (mosi & 0x3f) + 1
        self.current_transaction = Transaction(self.ss, Operation.WRITE, transfer_size)
        self.current_transaction.end_sample_op = self.es
        self.state = TransactionState.READ_ADDRESS

    def _transaction_read_addr(self, mosi, miso):
        # Get address bytes
        # Address is 3 bytes long
        self.current_transaction.address.extend(mosi.to_bytes(1, byteorder='big'))
        # The transfer size byte is sent at the same time than the last byte address
        if self.current_transaction.is_address_complete():
            self.current_transaction.end_sample_addr = self.es
            self.state = TransactionState.TRANSFER_DATA
            return

    def _transaction_data(self, mosi, miso):
        self.current_transaction.end_sample_data = self.es
        if miso == self.wait_mask:
            self.state = TransactionState.WAIT
            return
        if self.current_transaction.operation == Operation.READ:
            self.current_transaction.data.extend(miso.to_bytes(1, byteorder='big'))
            self.recover_vmk(miso)
        elif self.current_transaction.operation == Operation.WRITE:
            self.current_transaction.data.extend(mosi.to_bytes(1, byteorder='big'))
        # Check if the transaction is complete
        annotation = self.current_transaction.frame()
        if annotation:
            (op_ss, op_es, op_ann), (addr_ss, addr_es, addr_ann), (data_ss, data_es, data_ann) = annotation
            self.put(op_ss, op_es, self.out_ann,
                     [Ann.READ if self.current_transaction.operation == Operation.READ else Ann.WRITE, op_ann])
            self.put(addr_ss, addr_es, self.out_ann, [Ann.ADDRESS, addr_ann])
            self.put(data_ss, data_es, self.out_ann, [Ann.DATA, data_ann])
            self.end_current_transaction()

    def check_vmk_header(self):
        """ Check for VMK header """
        if self.queue[0] == 0x2c:
            potential_header = ''.join('{:02x}'.format(x) for x in self.queue)
            if re.findall(r'2c000[0-6]000[1-9]000[0-1]000[0-5]200000', potential_header):
                self.put(self.vmk_meta["s_queue"][0], self.es, self.out_ann,
                         [Ann.VMK, ['VMK header: {}'.format(potential_header)]])
                self.saving_vmk = True

    def recover_vmk(self, miso):
        """ Check if VMK is releasing """
        if not self.saving_vmk:
            # Add data to the circular buffer
            self.queue.append(miso)
            # Add sample number to meta queue
            self.vmk_meta["s_queue"].append(self.ss)
            # Check if VMK header retrieved
            self.check_vmk_header()
        else:
            if len(self.vmk) == 0:
                self.vmk_meta["vmk_ss"] = self.ss
            if len(self.vmk) < 32:
                self.vmk.append(miso)
                self.vmk_meta["vmk_es"] = self.es
            else:
                self.saving_vmk = False
                self.put(self.vmk_meta["vmk_ss"], self.vmk_meta["vmk_es"], self.out_ann,
                         [Ann.VMK, ['VMK: {}'.format(''.join('{:02x}'.format(x) for x in self.vmk))]])

    def decode(self, ss, es, data):
        self.wait_mask = bytes.fromhex(self.options['wait_mask'].strip("0x"))

        self.ss, self.es = ss, es

        ptype, mosi, miso = data

        if ptype == 'CS-CHANGE':
            self.end_current_transaction()

        if ptype != 'DATA':
            return

        if self.state is None:
            self.state = self.transaction_state(mosi)

        self.state_machine[self.state](mosi, miso)
