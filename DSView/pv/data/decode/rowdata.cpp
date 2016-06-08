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

#include <math.h>

#include "rowdata.h"

using std::max;
using std::min;
using std::vector;

namespace pv {
namespace data {
namespace decode {

RowData::RowData() :
    _max_annotation(0),
    _min_annotation(UINT64_MAX)
{
}

RowData::~RowData()
{
    _annotations.clear();
}

uint64_t RowData::get_max_sample() const
{
	if (_annotations.empty())
		return 0;
	return _annotations.back().end_sample();
}

uint64_t RowData::get_max_annotation() const
{
    return _max_annotation;
}

uint64_t RowData::get_min_annotation() const
{
    return _min_annotation;
}

void RowData::get_annotation_subset(
	vector<pv::data::decode::Annotation> &dest,
	uint64_t start_sample, uint64_t end_sample) const
{
	for (vector<Annotation>::const_iterator i = _annotations.begin();
		i != _annotations.end(); i++)
		if ((*i).end_sample() > start_sample &&
			(*i).start_sample() <= end_sample)
			dest.push_back(*i);
}

bool RowData::push_annotation(const Annotation &a)
{
    try {
      _annotations.push_back(a);
      _max_annotation = max(_max_annotation, a.end_sample() - a.start_sample());
      if (a.end_sample() != a.start_sample())
          _min_annotation = min(_min_annotation, a.end_sample() - a.start_sample());
      return true;
    } catch (const std::bad_alloc&) {
      return false;
    }
}

uint64_t RowData::get_annotation_size() const
{
    return _annotations.size();
}

bool RowData::get_annotation(Annotation &ann,
                             uint64_t index) const
{
    if (index < _annotations.size()) {
        ann = _annotations[index];
        return true;
    } else {
        return false;
    }
}

} // decode
} // data
} // pv
