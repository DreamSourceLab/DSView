/*
 * This file is part of the PulseView project.
 * DSView is based on PulseView.
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

#include <libsigrokdecode.h>
#include <assert.h>

namespace pv {
namespace data {
namespace decode {

Row::Row() :
	_decoder(NULL),
    _row(NULL),
    _order(-1)
{
}

Row::Row(const srd_decoder *decoder, const srd_decoder_annotation_row *row, const int order) :
	_decoder(decoder),
    _row(row),
    _order(order)
{
} 

Row::Row(const Row &o)
{
	_decoder = o._decoder;
	_row = o._row;
	_order = o._order;
}

Row& Row::operator=(const Row &o)
{
	_decoder = o._decoder;
	_row = o._row;
	_order = o._order;
	return (*this);
}

QString Row::title() const
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

QString Row::title_id() const
{
	if (_decoder && _decoder->id && _row && _row->desc)
		return QString("%1: %2")
			.arg(QString::fromUtf8(_decoder->id))
			.arg(QString::fromUtf8(_row->desc));

	if (_decoder && _decoder->id)
		return QString::fromUtf8(_decoder->id);

	if (_row && _row->desc)
		return QString::fromUtf8(_row->desc);

	return QString();
}

bool Row::operator<(const Row &other) const
{
    assert(_decoder);

    return (_decoder < other._decoder) ||
        (_decoder == other._decoder && _order < other._order);
}

} // decode
} // data
} // pv
