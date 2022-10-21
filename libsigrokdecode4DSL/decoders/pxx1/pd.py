##
## Created by VBKesha
## I don't know python syntax and written as can
##

import sigrokdecode as srd

class SamplerateError(Exception):
	pass

ann_byte, ann_bit, ann_bit_stuff, ann_desc_header, ann_desc_model_id, ann_desc_type, ann_desc_range_check, ann_desc_fail_safe, \
	ann_desc_country_code, ann_desc_bind, ann_desc_flag2, ann_desc_channes,ann_desc_rsrv, ann_desc_eu_plus, ann_desc_disable_sport, ann_desc_powerlevel, \
	ann_desc_rx_highchan, ann_desc_telemetry_off,ann_desc_external_antena, ann_desc_crc, ann_debug = range(21)

state_wait_header, state_rx_model_id, state_rx_type, state_rx_range_check, state_rx_fail_safe, state_rx_country_code, state_rx_bind, \
		state_rx_flag2, state_rx_channels, state_rx_rsrv2, state_rx_euplus, state_rx_disable_sport, state_rx_powerlevel, \
		state_rx_highchan, state_rx_telemetry_off, state_rx_external_antena, state_rx_crc, state_rx_stop, state_error = range(19)

PXX_HEADER          = 0x7E
PXX_SEND_BIND       = 0x01
PXX_SEND_FAILSAFE   = (1 << 4)
PXX_SEND_RANGECHECK = (1 << 5)

class Decoder(srd.Decoder):
	api_version = 3
	id = 'pxx1'
	name = 'PXX1'
	longname = 'PXX1 modulation'
	desc = 'FrSky PXX1(R9M) Protcol'
	license = 'Pirate'
	inputs = ['logic']
	outputs = []
	tags = ['PXX1']
	channels = (
		{'id': 'data', 'name': 'Data', 'desc': 'Data line', 'idn':'dec_pxx1_chan_data'},
	)
	options = ()

	annotations = (
		('byte', 'Byte'),						# Byte
		('bit', 'Bit'),							# Bit
		('bit_stuff', 'BitStuff'),				# BitStuff
		('start_header', 'Start Header'),
		('model_id', 'Model ID'),
		('type', 'Type'),
		('range_check', 'RangeCheck'),
		('fail_safe', 'FailSafe'),
		('country_code', 'CountryCode'),
		('bind', 'Bind'),
		('flags2', 'Flags2'),
		('channels', 'Channels'),
		('reserved', 'Reserved'),
		('is_euplpus', 'EU-PLUS'),
		('disable_sport', 'Disable SPort'),
		('power_level', 'Power Level'),
		('rx_highchan', 'Receive Hight Channel'),
		('telemetry_off', 'Telemetry Off'),
		('external_antena', 'ExternalAntena'),
		('CRC', 'CRC'),
	)
	
	annotation_rows = (
		 ('bytes', 'Bytes', (ann_byte,)),
		 ('bits', 'Bits', (ann_bit, ann_bit_stuff)),
		 ('desc', 'Description', (ann_desc_header,ann_desc_model_id,ann_desc_type,ann_desc_range_check,ann_desc_fail_safe,
		 		ann_desc_country_code,ann_desc_bind,ann_desc_flag2,ann_desc_channes,ann_desc_rsrv,ann_desc_eu_plus,ann_desc_disable_sport,ann_desc_powerlevel,
		 		ann_desc_rx_highchan, ann_desc_telemetry_off, ann_desc_external_antena, ann_desc_crc,)),
#		 ('dbg', 'DBG', (ann_debug,)),
	)

	transmit_type = ['FCC', 'EU', 'EU+', 'AU+']

	binary = (
		('raw', 'RAW file'),
	)

	byte = 0
	byte_cnt = 0
	cur_bit = 0
	bit_one_cnt = 0
	byte_start = 0
	bit_stuffing = False
	
	state_word = 0
	state_word_start = 0
	state_bit = 0
	state = state_wait_header
	nible_num = 0

	def __init__(self):
		self.reset()

	def reset(self):
		self.samplerate = None
		self.ss_block = self.es_block = None

	def metadata(self, key, value):
		if key == srd.SRD_CONF_SAMPLERATE:
			self.samplerate = value

	def start(self):
		self.out_ann = self.register(srd.OUTPUT_ANN)
		self.out_binary = self.register(srd.OUTPUT_BINARY)

	def breakRX(self):
		self.cur_bit = 0
		self.byte = 0
		self.byte_cnt = 0
		self.bit_stuffing = False
		self.nible = []
		self.state_word =0 
		self.state_bit = 0
		self.state = state_wait_header



	def addByte(self, value):
		self.put(self.byte_start, self.es_block, self.out_ann, [ann_byte, ["%02X" % value]])
		if value == 0x7E:
#			self.bit_stuffing = True
			self.bit_one_cnt = 0
#			self.put(self.byte_start, self.es_block, self.out_ann, [4, ["%s" % "Header"]])

		self.byte_cnt += 1
		if(self.byte_cnt > 18):
			self.bit_stuffing = False
			

	def addBit(self, value):
		bstuff = 0
		if self.cur_bit == 0:
			self.byte_start = self.ss_block 
		
		if self.state_bit == 0:
			self.state_word_start = self.ss_block

		self.bit_one_cnt += 1

		if (self.bit_stuffing == False) or (self.bit_one_cnt < 6):
			self.byte <<= 1
			self.byte |= value
			self.cur_bit += 1
			self.put(self.ss_block, self.es_block, self.out_ann, [ann_bit, ["%X" % value]])
			self.state_word <<= 1
			self.state_word |= value
			self.state_bit += 1
		else:
			bstuff = 1
			self.put(self.ss_block, self.es_block, self.out_ann, [ann_bit_stuff, ["%s" % "S"]])
		
		if value == 0:
			self.bit_one_cnt = 0

		if self.cur_bit == 8:
			self.addByte(self.byte)
			self.cur_bit = 0
			self.byte = 0

		if (self.state == state_wait_header):
			if(self.state_bit == 8):
				if(self.state_word == PXX_HEADER):
					self.put(self.state_word_start, self.es_block, self.out_ann, [ann_desc_header, ["%s" % "Start Header"]])
					self.state = state_rx_model_id
					self.state_bit = 0
					self.bit_stuffing = True
					self.state_word = 0
				else:
					self.put(self.state_word_start, self.es_block, self.out_ann, [ann_desc_header, ["%s" % "Header error"]])
					self.state = state_error
		
		if (self.state == state_rx_model_id):
			if(self.state_bit == 8):
				self.put(self.state_word_start, self.es_block, self.out_ann, [ann_desc_model_id, ["Model ID: %d" % self.state_word]])
				self.state = state_rx_type
				self.state_bit = 0
				self.state_word = 0

		if (self.state == state_rx_type):
			if(self.state_bit == 2):
				self.put(self.state_word_start, self.es_block, self.out_ann, [ann_desc_type, ["Type: %s" % self.transmit_type[self.state_word]]])
				self.state = state_rx_range_check
				self.state_bit = 0
				self.state_word = 0

		if (self.state == state_rx_range_check):
			if(self.state_bit == 1):
				self.put(self.state_word_start, self.es_block, self.out_ann, [ann_desc_range_check, ["Range Check: %s" % ("On" if self.state_word == 1 else "Off")]])
				self.state = state_rx_fail_safe
				self.state_bit = 0
				self.state_word = 0

		if (self.state == state_rx_fail_safe):
			if(self.state_bit == 1):
				self.put(self.state_word_start, self.es_block, self.out_ann, [ann_desc_fail_safe, ["FailSafe: %s" % ("On" if self.state_word == 1 else "Off")]])
				self.state = state_rx_country_code
				self.state_bit = 0
				self.state_word = 0

		if (self.state == state_rx_country_code):
			if(self.state_bit == 3):
				self.put(self.state_word_start, self.es_block, self.out_ann, [ann_desc_country_code, ["CountryCode: %u" % self.state_word]])
				self.state = state_rx_bind
				self.state_bit = 0
				self.state_word = 0

		if (self.state == state_rx_bind):
			if(self.state_bit == 1):
				self.put(self.state_word_start, self.es_block, self.out_ann, [ann_desc_bind, ["Bind: %s" % ("On" if self.state_word == 1 else "Off")]])
				self.state = state_rx_flag2
				self.state_bit = 0
				self.state_word = 0

		if (self.state == state_rx_flag2):
			if(self.state_bit == 8):
				self.put(self.state_word_start, self.es_block, self.out_ann, [ann_desc_flag2, ["Flag2: %u" % self.state_word]])
				self.state = state_rx_channels
				self.nible = []
				self.state_bit = 0
				self.state_word = 0

		if (self.state == state_rx_channels):
			if((bstuff == 0) and (self.state_bit > 0) and (self.state_bit % 4 == 0)):
				self.nible.append(self.state_word)
				self.state_word = 0

			if(self.state_bit == 96):
				chans = []
				is_upper = False
				out = ""
				while (len(self.nible) > 0):
					ch1 = self.nible[3] << 8 | self.nible[1] << 4 | self.nible[0];
					ch2 = self.nible[4] << 8 | self.nible[5] << 4 | self.nible[2];
					out += "%04u %04u" % (ch1, ch2)
					for x in range(0, 6):
						self.nible.pop(0)

					if(len(self.nible) > 0):
						out += " "
					if(ch1 > 2048):
						is_upper = True


				self.put(self.state_word_start, self.es_block, self.out_ann, [ann_desc_channes, ["Channels %s: [%s]" % ("(1-8)" if is_upper == False else "(9-16)", out)]])
				self.state_bit = 0
				self.state_word = 0
				self.state = state_rx_rsrv2
		
				#self.nible_num += 1
				#if(self.nible_num == 24):
				#	self.put(self.state_word_start, self.es_block, self.out_ann, [ann_desc_channes, ["Chan: %u" % 123]])
				#	self.state = state_rx_rsrv2
		
		if (self.state == state_rx_rsrv2):
			if(self.state_bit == 1):
				self.put(self.state_word_start, self.es_block, self.out_ann, [ann_desc_rsrv, ["Reserved"]])
				self.state = state_rx_euplus
				self.state_bit = 0
				self.state_word = 0


		if (self.state == state_rx_euplus):
			if(self.state_bit == 1):
				self.put(self.state_word_start, self.es_block, self.out_ann, [ann_desc_eu_plus, ["EUPlus: %s" % ("Yes" if self.state_word == 1 else "No")]])
				self.state = state_rx_disable_sport
				self.state_bit = 0
				self.state_word = 0
		
		if (self.state == state_rx_disable_sport):
			if(self.state_bit == 1):
				self.put(self.state_word_start, self.es_block, self.out_ann, [ann_desc_disable_sport, ["SPort: %s" % ("Disabled" if self.state_word == 1 else "Enable")]])
				self.state = state_rx_powerlevel
				self.state_bit = 0
				self.state_word = 0	
	
		if (self.state == state_rx_powerlevel):
			if(self.state_bit == 2):
				self.put(self.state_word_start, self.es_block, self.out_ann, [ann_desc_powerlevel, ["PowerLevel: %u" % self.state_word ]])
				self.state = state_rx_highchan
				self.state_bit = 0
				self.state_word = 0	

	
		if (self.state == state_rx_highchan):
			if(self.state_bit == 1):
				self.put(self.state_word_start, self.es_block, self.out_ann, [ann_desc_rx_highchan, ["RX HighChannel: %s" % ("Yes" if self.state_word == 1 else "No")]])
				self.state = state_rx_telemetry_off
				self.state_bit = 0
				self.state_word = 0	

		if (self.state == state_rx_telemetry_off):
			if(self.state_bit == 1):
				self.put(self.state_word_start, self.es_block, self.out_ann, [ann_desc_telemetry_off, ["Telemetry: %s" % ("Off" if self.state_word == 1 else "On")]])
				self.state = state_rx_external_antena
				self.state_bit = 0
				self.state_word = 0	

		if (self.state == state_rx_external_antena):
			if(self.state_bit == 1):
				self.put(self.state_word_start, self.es_block, self.out_ann, [ann_desc_external_antena, ["ExternalAntena: %s" % ("Yes" if self.state_word == 1 else "No")]])
				self.state = state_rx_crc
				self.state_bit = 0
				self.state_word = 0	

		if (self.state == state_rx_crc):
			if(self.state_bit == 16):
				self.put(self.state_word_start, self.es_block, self.out_ann, [ann_desc_crc, ["CRC: 0x%04X" % self.state_word]])
				self.state = state_rx_stop
				self.state_bit = 0
				self.state_word = 0	
				self.bit_stuff = False

		if (self.state == state_rx_stop):
			if(self.state_bit == 8):
				if(self.state_word == PXX_HEADER):
					self.put(self.state_word_start, self.es_block, self.out_ann, [ann_desc_header, ["%s" % "Stop Header"]])
					self.state = state_wait_header
					self.state_bit = 0
					self.state_word = 0
				else:
					self.put(self.state_word_start, self.es_block, self.out_ann, [ann_desc_header, ["%s" % "Header error"]])
					self.state = state_error


	def decode(self):
		if not self.samplerate:
			raise SamplerateError('Cannot decode without samplerate.')

		num_cycles = 0
		average = 0

		# Wait for an "active" edge (depends on config). This starts
		# the first full period of the inspected signal waveform.
		self.wait({0: 'f'})
		self.first_samplenum = self.samplenum

		# Keep getting samples for the period's middle and terminal edges.
		# At the same time that last sample starts the next period.
		
		while True:

			# Get the next two edges. Setup some variables that get
			# referenced in the calculation and in put() routines.
			start_samplenum = self.samplenum
			self.wait({0: 'r'})
			end_samplenum = self.samplenum
			self.wait({0: 'f'})
			self.ss_block = start_samplenum
			self.es_block = self.samplenum

			# Calculate the period, the duty cycle, and its ratio.
			period = self.samplenum - start_samplenum
			duty = end_samplenum - start_samplenum
			ratio = float(duty / period)

			# Report the duty cycle in percent.
			# percent = float(ratio * 100)
			# self.putx([0, ['%f%%' % percent]])

			# Report the period in units of time.
			period_t = float(period / self.samplerate)
			
			if period_t>=0.000023 and period_t<= 0.000025:
				#self.put(self.ss_block, self.es_block, self.out_ann, [1, ["1"]])
				self.addBit(1)

			if period_t>=0.000015 and period_t<=0.000017:
				#self.put(self.ss_block, self.es_block, self.out_ann, [1, ["0"]])
				self.addBit(0)

			if period_t>=0.000040:
				self.es_block = self.ss_block + int(self.samplerate / 1000000 * 16) # dont doit that
				#self.put(self.ss_block, self.es_block , self.out_ann, [1, ["0"]])
				self.addBit(0)
				self.breakRX()