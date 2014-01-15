/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2010-2012 Bert Vermeulen <bert@biot.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIBSIGROK_OUTPUT_TEXT_TEXT_H
#define LIBSIGROK_OUTPUT_TEXT_TEXT_H

#define DEFAULT_BPL_BITS 64
#define DEFAULT_BPL_HEX  192
#define DEFAULT_BPL_ASCII 74

enum outputmode {
	MODE_BITS = 1,
	MODE_HEX,
	MODE_ASCII,
};

struct context {
	unsigned int num_enabled_probes;
	int samples_per_line;
	unsigned int unitsize;
	int line_offset;
	int linebuf_len;
	GSList *probenames;
	uint8_t *linebuf;
	int spl_cnt;
	uint8_t *linevalues;
	char *header;
	int mark_trigger;
	uint8_t *prevsample;
	enum outputmode mode;
};

SR_PRIV void flush_linebufs(struct context *ctx, uint8_t *outbuf);
SR_PRIV int init(struct sr_output *o, int default_spl, enum outputmode mode);
SR_PRIV int event(struct sr_output *o, int event_type, uint8_t **data_out,
		  uint64_t *length_out);

SR_PRIV int init_bits(struct sr_output *o);
SR_PRIV int data_bits(struct sr_output *o, const uint8_t *data_in,
		      uint64_t length_in, uint8_t **data_out,
		      uint64_t *length_out);

SR_PRIV int init_hex(struct sr_output *o);
SR_PRIV int data_hex(struct sr_output *o, const uint8_t *data_in,
		     uint64_t length_in, uint8_t **data_out,
		     uint64_t *length_out);

SR_PRIV int init_ascii(struct sr_output *o);
SR_PRIV int data_ascii(struct sr_output *o, const uint8_t *data_in,
		       uint64_t length_in, uint8_t **data_out,
		       uint64_t *length_out);

#endif
