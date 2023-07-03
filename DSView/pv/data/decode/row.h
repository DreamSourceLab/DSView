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

#ifndef DSVIEW_PV_DATA_DECODE_ROW_H
#define DSVIEW_PV_DATA_DECODE_ROW_H

#include <vector>

#include "annotation.h"

struct srd_decoder;
struct srd_decoder_annotation_row;

namespace pv {
namespace data {
namespace decode {

//map find key data
class Row
{
public:
	Row();

	Row(const srd_decoder *decoder,
        const srd_decoder_annotation_row *row = NULL,
        const int order = -1);

	Row(const Row &o);

	Row& operator=(const Row &o);

public: 

	inline const srd_decoder* decoder() const{
		return _decoder;
	}

	inline const srd_decoder_annotation_row* row() const{
		return _row;
	}

    QString title() const;

	QString title_id() const;

    bool operator<(const Row &other)const;

private:
	const srd_decoder *_decoder;
	const srd_decoder_annotation_row *_row;
    int _order; 
};

} // decode
} // data
} // pv

#endif // DSVIEW_PV_DATA_DECODE_ROW_H
