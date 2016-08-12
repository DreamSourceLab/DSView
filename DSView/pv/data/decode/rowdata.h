/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2014 Joel Holdsworth <joel@airwebreathe.org.uk>
 * Copyright (C) 2016 DreamSourceLab <support@dreamsourcelab.com>
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

#ifndef DSVIEW_PV_DATA_DECODE_ROWDATA_H
#define DSVIEW_PV_DATA_DECODE_ROWDATA_H

#include <vector>

#include "annotation.h"

namespace pv {
namespace data {
namespace decode {

class RowData
{
public:
	RowData();
    ~RowData();
public:
	uint64_t get_max_sample() const;

    uint64_t get_max_annotation() const;
    uint64_t get_min_annotation() const;
	/**
	 * Extracts sorted annotations between two period into a vector.
	 */
	void get_annotation_subset(
		std::vector<pv::data::decode::Annotation> &dest,
		uint64_t start_sample, uint64_t end_sample) const;

    bool push_annotation(const Annotation &a);

    uint64_t get_annotation_size() const;

    bool get_annotation(pv::data::decode::Annotation &ann,
                        uint64_t index) const;

private:
    uint64_t _max_annotation;
    uint64_t _min_annotation;
	std::vector<Annotation> _annotations;
};

}
} // data
} // pv

#endif // DSVIEW_PV_DATA_DECODE_ROWDATA_H
