/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2014 Joel Holdsworth <joel@airwebreathe.org.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include "row.h"

#include <libsigrokdecode/libsigrokdecode.h>

namespace pv {
namespace data {
namespace decode {

Row::Row() :
	_decoder(NULL),
	_row(NULL)
{
}

Row::Row(const srd_decoder *decoder, const srd_decoder_annotation_row *row) :
	_decoder(decoder),
	_row(row)
{
}

const srd_decoder* Row::decoder() const
{
	return _decoder;
}

const srd_decoder_annotation_row* Row::row() const
{
	return _row;
}

const QString Row::title() const
{
	if (_decoder && _decoder->name && _row && _row->desc)
		return QString("%1: %2")
			.arg(QString::fromUtf8(_decoder->name))
			.arg(QString::fromUtf8(_row->desc));
	if (_decoder && _decoder->name)
		return QString::fromUtf8(_decoder->name);
	if (_row && _row->desc)
		return QString::fromUtf8(_row->desc);
	return QString();
}

bool Row::operator<(const Row &other) const
{
	return (_decoder < other._decoder) ||
		(_decoder == other._decoder && _row < other._row);
}

} // decode
} // data
} // pv
