/*
 * This file is part of the DSLogic-gui project.
 * DSLogic-gui is based on PulseView.
 *
 * Copyright (C) 2012 Joel Holdsworth <joel@airwebreathe.org.uk>
 * Copyright (C) 2013 DreamSourceLab <dreamsourcelab@dreamsourcelab.com>
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


#include <extdef.h>

#include <math.h>

#include "groupsignal.h"
#include "pv/data/group.h"
#include "pv/data/groupsnapshot.h"

using namespace boost;
using namespace std;

namespace pv {
namespace view {

const QColor GroupSignal::SignalColours[4] = {
	QColor(0xC4, 0xA0, 0x00),	// Yellow
	QColor(0x87, 0x20, 0x7A),	// Magenta
	QColor(0x20, 0x4A, 0x87),	// Blue
	QColor(0x4E, 0x9A, 0x06)	// Green
};

const float GroupSignal::EnvelopeThreshold = 256.0f;

GroupSignal::GroupSignal(QString name, boost::shared_ptr<data::Group> data,
                         std::list<int> probe_index_list, int order, int group_index) :
    Signal(name, probe_index_list, DS_GROUP, order, group_index),
    _data(data)
{
    _colour = SignalColours[probe_index_list.front() % countof(SignalColours)];
    _scale = _signalHeight * 1.0f / pow(2, probe_index_list.size());
}

GroupSignal::~GroupSignal()
{
}

void GroupSignal::set_data(boost::shared_ptr<data::Logic> _logic_data,
                           boost::shared_ptr<data::Dso> _dso_data,
                           boost::shared_ptr<pv::data::Analog> _analog_data,
                           boost::shared_ptr<data::Group> _group_data)
{
    (void)_logic_data;
    (void)_dso_data;
    (void)_analog_data;

    assert(_group_data);

    _data = _group_data;
}

void GroupSignal::set_scale(float scale)
{
	_scale = scale;
}

void GroupSignal::paint(QPainter &p, int y, int left, int right, double scale,
    double offset)
{
	assert(scale > 0);
	assert(_data);
	assert(right >= left);

    _scale = _signalHeight * 1.0f / pow(2, _index_list.size());
	paint_axis(p, y, left, right);

    const deque< boost::shared_ptr<pv::data::GroupSnapshot> > &snapshots =
		_data->get_snapshots();
    if (snapshots.empty())
		return;

    const boost::shared_ptr<pv::data::GroupSnapshot> &snapshot =
            snapshots.at(_sec_index);

	const double pixels_offset = offset / scale;
	const double samplerate = _data->get_samplerate();
	const double start_time = _data->get_start_time();
    const int64_t last_sample = snapshot->get_sample_count() - 1;
	const double samples_per_pixel = samplerate * scale;
	const double start = samplerate * (offset - start_time);
	const double end = start + samples_per_pixel * (right - left);

	const int64_t start_sample = min(max((int64_t)floor(start),
		(int64_t)0), last_sample);
	const int64_t end_sample = min(max((int64_t)ceil(end) + 1,
		(int64_t)0), last_sample);

	if (samples_per_pixel < EnvelopeThreshold)
        paint_trace(p, snapshot, y, left,
			start_sample, end_sample,
			pixels_offset, samples_per_pixel);
	else
        paint_envelope(p, snapshot, y, left,
			start_sample, end_sample,
			pixels_offset, samples_per_pixel);
}

void GroupSignal::paint_trace(QPainter &p,
    const boost::shared_ptr<pv::data::GroupSnapshot> &snapshot,
	int y, int left, const int64_t start, const int64_t end,
	const double pixels_offset, const double samples_per_pixel)
{
	const int64_t sample_count = end - start;

    const uint16_t *samples = snapshot->get_samples(start, end);
	assert(samples);

	p.setPen(_colour);

	QPointF *points = new QPointF[sample_count];
	QPointF *point = points;

	for (int64_t sample = start; sample != end; sample++) {
		const float x = (sample / samples_per_pixel -
			pixels_offset) + left;
		*point++ = QPointF(x,
			y - samples[sample - start] * _scale);
	}

	p.drawPolyline(points, point - points);

	delete[] samples;
	delete[] points;
}

void GroupSignal::paint_envelope(QPainter &p,
    const boost::shared_ptr<pv::data::GroupSnapshot> &snapshot,
	int y, int left, const int64_t start, const int64_t end,
	const double pixels_offset, const double samples_per_pixel)
{
	using namespace Qt;
    using pv::data::GroupSnapshot;

    GroupSnapshot::EnvelopeSection e;
	snapshot->get_envelope_section(e, start, end, samples_per_pixel);

	if (e.length < 2)
		return;

	p.setPen(QPen(NoPen));
	p.setBrush(_colour);

	QRectF *const rects = new QRectF[e.length];
	QRectF *rect = rects;

	for(uint64_t sample = 0; sample < e.length-1; sample++) {
		const float x = ((e.scale * sample + e.start) /
			samples_per_pixel - pixels_offset) + left;
        const GroupSnapshot::EnvelopeSample *const s =
			e.samples + sample;

		// We overlap this sample with the next so that vertical
		// gaps do not appear during steep rising or falling edges
		const float b = y - max(s->max, (s+1)->min) * _scale;
		const float t = y - min(s->min, (s+1)->max) * _scale;

		float h = b - t;
		if(h >= 0.0f && h <= 1.0f)
			h = 1.0f;
		if(h <= 0.0f && h >= -1.0f)
			h = -1.0f;

		*rect++ = QRectF(x, t, 1.0f, h);
	}

	p.drawRects(rects, e.length);

	delete[] rects;
	delete[] e.samples;
}

const std::vector< std::pair<uint64_t, bool> > GroupSignal::cur_edges() const
{

}

void GroupSignal::set_decoder(pv::decoder::Decoder *decoder)
{
    (void)decoder;
}

decoder::Decoder *GroupSignal::get_decoder()
{
    return NULL;
}

void GroupSignal::del_decoder()
{
}

} // namespace view
} // namespace pv
