/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2013 Joel Holdsworth <joel@airwebreathe.org.uk>
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

#ifndef DSVIEW_PV_VIEW_DECODE_ANNOTATION_H
#define DSVIEW_PV_VIEW_DECODE_ANNOTATION_H

#include <stdint.h>

#include <QString>

struct srd_proto_data;

namespace pv {
namespace data {
namespace decode {

class Annotation
{
public:
	Annotation(const srd_proto_data *const pdata);
    Annotation();
    ~Annotation();

	uint64_t start_sample() const;
	uint64_t end_sample() const;
	int format() const;
    int type() const;
	const std::vector<QString>& annotations() const;

private:
	uint64_t _start_sample;
	uint64_t _end_sample;
	int _format;
    int _type;
	std::vector<QString> _annotations; 
};

} // namespace decode
} // namespace data
} // namespace pv

#endif // DSVIEW_PV_VIEW_DECODE_ANNOTATION_H
