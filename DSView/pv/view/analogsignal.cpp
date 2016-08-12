/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
 * Copyright (C) 2012 Joel Holdsworth <joel@airwebreathe.org.uk>
 * Copyright (C) 2013 DreamSourceLab <support@dreamsourcelab.com>
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

#include "analogsignal.h"
#include "pv/data/analog.h"
#include "pv/data/analogsnapshot.h"
#include "view.h"

using namespace boost;
using namespace std;

namespace pv {
namespace view {

//const QColor AnalogSignal::SignalColours[4] = {
//	QColor(0xC4, 0xA0, 0x00),	// Yellow
//	QColor(0x87, 0x20, 0x7A),	// Magenta
//	QColor(0x20, 0x4A, 0x87),	// Blue
//	QColor(0x4E, 0x9A, 0x06)	// Green
//};
const QColor AnalogSignal::SignalColours[4] = {
    QColor(17, 133, 209,  255), // dsBlue
    QColor(238, 178, 17, 255),  // dsYellow
    QColor(213, 15, 37, 255),   // dsRed
    QColor(0, 153, 37, 255)     // dsGreen
};

const float AnalogSignal::EnvelopeThreshold = 256.0f;

AnalogSignal::AnalogSignal(boost::shared_ptr<pv::device::DevInst> dev_inst,
                           boost::shared_ptr<data::Analog> data,
                           sr_channel *probe) :
    Signal(dev_inst, probe),
    _data(data)
{
    _typeWidth = 3;
    _colour = SignalColours[probe->index % countof(SignalColours)];
    _scale = _totalHeight * 1.0f / 65536;
}

AnalogSignal::~AnalogSignal()
{
}

boost::shared_ptr<pv::data::SignalData> AnalogSignal::data() const
{
    return _data;
}

void AnalogSignal::set_scale(float scale)
{
	_scale = scale;
}

void AnalogSignal::paint_mid(QPainter &p, int left, int right)
{
    assert(_data);
    assert(_view);
    assert(right >= left);

    const int y = get_y() + _totalHeight * 0.5;
    const double scale = _view->scale();
    assert(scale > 0);
    const double offset = _view->offset();

    const deque< boost::shared_ptr<pv::data::AnalogSnapshot> > &snapshots =
		_data->get_snapshots();
	if (snapshots.empty())
		return;

    _scale = _totalHeight * 1.0f / 65536;
	const boost::shared_ptr<pv::data::AnalogSnapshot> &snapshot =
		snapshots.front();
    if (snapshot->empty())
        return;

    if (get_index() >= (int)snapshot->get_channel_num())
        return;

	const double pixels_offset = offset / scale;
    const double samplerate = _data->samplerate();
	const double start_time = _data->get_start_time();
    const int64_t last_sample = max((int64_t)(snapshot->get_sample_count() - 1), (int64_t)0);
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

void AnalogSignal::paint_trace(QPainter &p,
	const boost::shared_ptr<pv::data::AnalogSnapshot> &snapshot,
	int y, int left, const int64_t start, const int64_t end,
	const double pixels_offset, const double samples_per_pixel)
{
	const int64_t sample_count = end - start;
    const int64_t channel_num = snapshot->get_channel_num();

    if (sample_count > 0) {
        const uint16_t *const samples = snapshot->get_samples(start, end);
        assert(samples);

        p.setPen(_colour);
        //p.setPen(QPen(_colour, 2, Qt::SolidLine));

        QPointF *points = new QPointF[sample_count];
        QPointF *point = points;        
        for (int64_t sample = start; sample != end; sample++) {
            const float x = (sample / samples_per_pixel -
                pixels_offset) + left;
            *point++ = QPointF(x,
                               y - samples[(sample - start) * channel_num + get_index()] * _scale);
        }

        p.drawPolyline(points, point - points);
        //delete[] samples;
        delete[] points;
    }
}

void AnalogSignal::paint_envelope(QPainter &p,
	const boost::shared_ptr<pv::data::AnalogSnapshot> &snapshot,
	int y, int left, const int64_t start, const int64_t end,
	const double pixels_offset, const double samples_per_pixel)
{
	using namespace Qt;
	using pv::data::AnalogSnapshot;

	AnalogSnapshot::EnvelopeSection e;
    snapshot->get_envelope_section(e, start, end, samples_per_pixel, get_index());

	if (e.length < 2)
		return;

    p.setPen(QPen(NoPen));
    //p.setPen(QPen(_colour, 2, Qt::SolidLine));
    p.setBrush(_colour);

	QRectF *const rects = new QRectF[e.length];
	QRectF *rect = rects;

	for(uint64_t sample = 0; sample < e.length-1; sample++) {
		const float x = ((e.scale * sample + e.start) /
			samples_per_pixel - pixels_offset) + left;
		const AnalogSnapshot::EnvelopeSample *const s =
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
    //delete[] e.samples;
}

const std::vector< std::pair<uint64_t, bool> > AnalogSignal::cur_edges() const
{

}

} // namespace view
} // namespace pv
