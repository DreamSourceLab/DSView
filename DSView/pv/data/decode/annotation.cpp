/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2012 Joel Holdsworth <joel@airwebreathe.org.uk>
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

extern "C" {
#include <libsigrokdecode/libsigrokdecode.h>
}

#include <vector>
#include <assert.h>

#include "annotation.h"

namespace pv {
namespace data {
namespace decode {

Annotation::Annotation(const srd_proto_data *const pdata) :
	_start_sample(pdata->start_sample),
	_end_sample(pdata->end_sample)
{
	assert(pdata);
	const srd_proto_data_annotation *const pda =
		(const srd_proto_data_annotation*)pdata->data;
	assert(pda);

    _format = pda->ann_class;
    _type = pda->ann_type;

    const char *const *annotations = (char**)pda->ann_text;
	while(*annotations) {
        _annotations.push_back(QString::fromUtf8(*annotations));
		annotations++;
	}
}

Annotation::Annotation()
{
    _start_sample = 0;
    _end_sample = 0;
}

Annotation::~Annotation()
{
    _annotations.clear();
}

uint64_t Annotation::start_sample() const
{
	return _start_sample;
}

uint64_t Annotation::end_sample() const
{
	return _end_sample;
}

int Annotation::format() const
{
	return _format;
}

int Annotation::type() const
{
    return _type;
}

const std::vector<QString>& Annotation::annotations() const
{
	return _annotations;
}

} // namespace decode
} // namespace data
} // namespace pv
