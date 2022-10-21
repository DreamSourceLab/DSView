##
## This file is part of the libsigrokdecode project.
##
## Copyright (C) 2013-2016 Uwe Hermann <uwe@hermann-uwe.de>
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
from common.srdhelper import bitpack

'''
OUTPUT_PYTHON format:

Packet:
[<ptype>, <pdata>]

<ptype>, <pdata>
 - 'ITEM', [<item>, <itembitsize>]
 - 'WORD', [<word>, <wordbitsize>, <worditemcount>]

<item>:
 - A single item (a number). It can be of arbitrary size. The max. number
   of bits in this item is specified in <itembitsize>.

<itembitsize>:
 - The size of an item (in bits). For a 4-bit parallel bus this is 4,
   for a 16-bit parallel bus this is 16, and so on.

<word>:
 - A single word (a number). It can be of arbitrary size. The max. number
   of bits in this word is specified in <wordbitsize>. The (exact) number
   of items in this word is specified in <worditemcount>.

<wordbitsize>:
 - The size of a word (in bits). For a 2-item word with 8-bit items
   <wordbitsize> is 16, for a 3-item word with 4-bit items <wordbitsize>
   is 12, and so on.

<worditemcount>:
 - The size of a word (in number of items). For a 4-item word (no matter
   how many bits each item consists of) <worditemcount> is 4, for a 7-item
   word <worditemcount> is 7, and so on.
'''

def channel_list(num_channels):
    l = [{'id': 'clk', 'name': 'CLK', 'desc': 'Clock line'}]
    for i in range(num_channels):
        d = {'id': 'd%d' % i, 'name': 'D%d' % i, 'desc': 'Data line %d' % i}
        l.append(d)
    return tuple(l)

class ChannelError(Exception):
    pass

NUM_CHANNELS = 32

class Decoder(srd.Decoder):
    api_version = 3
    id = 'parallel'
    name = 'Parallel'
    longname = 'Parallel sync bus'
    desc = 'Generic parallel synchronous bus.'
    license = 'gplv2+'
    inputs = ['logic']
    outputs = ['parallel']
    tags = ['Util']
    optional_channels = channel_list(NUM_CHANNELS)
    options = (
        {'id': 'clock_edge', 'desc': 'Clock edge to sample on',
            'default': 'rising', 'values': ('rising', 'falling'), 'idn':'dec_parallel_opt_clock_edge'},
        {'id': 'wordsize', 'desc': 'Data wordsize (# bus cycles)',
            'default': 0, 'idn':'dec_parallel_opt_wordsize'},
        {'id': 'endianness', 'desc': 'Data endianness',
            'default': 'little', 'values': ('little', 'big'), 'idn':'dec_parallel_opt_endianness'},
    )
    annotations = (
        ('items', 'Items'),
        ('words', 'Words'),
    )
    annotation_rows = (
        ('items', 'Items', (0,)),
        ('words', 'Words', (1,)),
    )

    def __init__(self):
        self.reset()

    def reset(self):
        self.items = []
        self.saved_item = None
        self.saved_word = None
        self.ss_word = self.es_word = None
        self.first = True 
        self.have_clock = True
        self.prv_dex = 0
        self.num_item_bits = None

    def start(self):
        self.out_python = self.register(srd.OUTPUT_PYTHON)
        self.out_ann = self.register(srd.OUTPUT_ANN)

    def putpw(self, data):
        self.put(self.ss_word, self.es_word, self.out_python, data)

    def putw(self, data):
        self.put(self.ss_word, self.es_word, self.out_ann, data)

    def put_ann(self, s, e, data):
        self.put(s, e, self.out_ann, data)
    
    def put_py(self, s, e, data):
        self.put(s, e, self.out_python, data)

    def handle_bits(self, item):
        # If a word was previously accumulated, then emit its annotation
        # now after its end samplenumber became available.
        cur_dex = self.samplenum

        # Defer annotations for individual items until the next sample
        # is taken, and the previous sample's end samplenumber has
        # become available. 
        if self.first:
            # Save the start sample and item for later (no output yet).
            if not self.have_clock:
                self.put_py(self.prv_dex, cur_dex, ['ITEM', self.saved_item])
                self.put_ann(self.prv_dex, cur_dex, [0, [self.fmt_item.format(self.saved_item)]])
             
            self.first = False
            self.saved_item = item
        else:            
            # Output the saved item (from the last CLK edge to the current).
            self.put_py(self.prv_dex, cur_dex, ['ITEM', self.saved_item])
            self.put_ann(self.prv_dex, cur_dex, [0, [self.fmt_item.format(self.saved_item)]])        
            self.saved_item = item

        self.prv_dex = cur_dex
        self.handel_word(item, cur_dex)

    #word 
    def handel_word(self, item, cur_dex):
        if self.saved_word is not None:
            if self.options['wordsize'] > 0:
                self.es_word = cur_dex
                self.putw([1, [self.fmt_word.format(self.saved_word)]])
                self.putpw(['WORD', self.saved_word])
            self.saved_word = None

        if item is None:
            return

         # Get as many items as the configured wordsize specifies.
        if not self.items:
            self.ss_word = cur_dex

        self.items.append(item)
        ws = self.options['wordsize']

        if len(self.items) < ws:
            return

        # Collect words and prepare annotation details, but defer emission
        # until the end samplenumber becomes available.
        endian = self.options['endianness']

        if endian == 'big':
            self.items.reverse()

        word = sum([self.items[i] << (i * self.num_item_bits) for i in range(ws)])
        self.saved_word = word
        self.items = []

    def end(self): 
        cur_dex = self.last_samplenum
        #the last annotation
        if self.saved_item != None:
            self.put_py(self.prv_dex, cur_dex, ['ITEM', self.saved_item])
            self.put_ann(self.prv_dex, cur_dex, [0, [self.fmt_item.format(self.saved_item)]])
            self.handel_word(None, cur_dex)

    def decode(self):
        # Determine which (optional) channels have input data. Insist in
        # a non-empty input data set. Cope with sparse connection maps.
        # Store enough state to later "compress" sampled input data.
        max_possible = len(self.optional_channels)

        idx_channels = [
            idx if self.has_channel(idx) else None
            for idx in range(max_possible)
        ]
        
        has_channels = [idx for idx in idx_channels if idx is not None]
        if not has_channels:
            raise ChannelError('At least one channel has to be supplied.')
        max_connected = max(has_channels)

        self.have_clock = self.has_channel(0)
        self.prv_dex = self.samplenum
        have_clock = self.have_clock
    
        # Determine .wait() conditions, depending on the presence of a
        # clock signal. Either inspect samples on the configured edge of
        # the clock, or inspect samples upon ANY edge of ANY of the pins
        # which provide input data.
        if have_clock:
            edge = self.options['clock_edge'][0]
            conds = {0: edge} #'f' or 'r'
        else:
            conds = [{idx: 'e'} for idx in has_channels]
        
        # Pre-determine which input data to strip off, the width of
        # individual items and multiplexed words, as well as format
        # strings here. This simplifies call sites which run in tight
        # loops later.
        idx_strip = max_connected + 1
        num_item_bits = idx_strip - 1
        num_word_items = self.options['wordsize']
        num_word_bits = num_item_bits * num_word_items
        num_digits = (num_item_bits + 3) // 4
        self.fmt_item = "@{{:0{}X}}".format(num_digits)
        num_digits = (num_word_bits + 3) // 4
        self.fmt_word = "@{{:0{}X}}".format(num_digits)
        self.num_item_bits = num_item_bits

        # Keep processing the input stream. Assume "always zero" for
        # not-connected input lines. Pass data bits (all inputs except
        # clock) to the handle_bits() method.

        is_first = True
        the_conds = conds

        while True:
            if not have_clock and is_first:
                #get the value at sample 0
                conds = None
            else:
                conds = the_conds

            (clk, d0, d1, d2, d3, d4, d5, d6, d7,d8, d9,d10 ,d11 ,d12 ,d13 ,d14 ,d15 ,d16 ,d17 ,d18 ,d19 ,d20 ,d21 ,d22 ,d23 ,d24 ,d25 ,d26 ,d27 ,d28 ,d29 ,d30 ,d31 ) = self.wait(conds)
            pins = (clk, d0, d1, d2, d3, d4, d5, d6, d7,d8, d9, d10, d11, d12,d13 ,d14 ,d15 ,d16 ,d17 ,d18 ,d19 ,d20 ,d21 ,d22 ,d23 ,d24 ,d25 ,d26 ,d27 ,d28 ,d29 ,d30 ,d31 )
            bits = [0 if idx is None else pins[idx] for idx in idx_channels]
            item = bitpack(bits[1:idx_strip])

            if not have_clock and is_first:
                is_first = False 
                self.saved_item = item
                continue

            self.handle_bits(item)
