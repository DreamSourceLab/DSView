/*
 * This file is part of the DSLogic-gui project.
 * DSLogic-gui is based on PulseView.
 *
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

#include "dsosignal.h"
#include "pv/data/dso.h"
#include "pv/data/dsosnapshot.h"

using namespace boost;
using namespace std;

namespace pv {
namespace view {

const QColor DsoSignal::SignalColours[4] = {
    QColor(238, 178, 17, 200),  // dsYellow
    QColor(0, 153, 37, 200),    // dsGreen
    QColor(213, 15, 37, 200),   // dsRed
    QColor(17, 133, 209, 200)  // dsBlue

};

const float DsoSignal::EnvelopeThreshold = 256.0f;

DsoSignal::DsoSignal(QString name, boost::shared_ptr<data::Dso> data,
    int probe_index, int order, uint64_t vdiv, uint64_t timebase, bool coupling, bool active) :
    Signal(name, probe_index, DS_DSO, order),
    _data(data)
{
	_colour = SignalColours[probe_index % countof(SignalColours)];
    _scale = _windowHeight * 1.0f / 256;
    _vDial->set_value(vdiv);
    _hDial->set_value(timebase);
    _acCoupling = coupling;
    _active = active;
}

DsoSignal::~DsoSignal()
{
}

void DsoSignal::set_data(boost::shared_ptr<data::Logic> _logic_data,
                            boost::shared_ptr<data::Dso> _dso_data,
                            boost::shared_ptr<pv::data::Analog> _analog_data,
                            boost::shared_ptr<data::Group> _group_data)
{
    (void)_analog_data;
    (void)_logic_data;
    (void)_group_data;

    assert(_dso_data);

    _data = _dso_data;
}

void DsoSignal::set_scale(float scale)
{
	_scale = scale;
}

void DsoSignal::paint(QPainter &p, int y, int left, int right, double scale,
    double offset)
{
	assert(scale > 0);
	assert(_data);
	assert(right >= left);

    const deque< boost::shared_ptr<pv::data::DsoSnapshot> > &snapshots =
		_data->get_snapshots();
	if (snapshots.empty())
		return;

    _scale = _windowHeight * 1.0f / 256;
    const boost::shared_ptr<pv::data::DsoSnapshot> &snapshot =
		snapshots.front();

    const uint16_t number_channels = snapshot->get_channel_num();
    if ((unsigned int)get_index() >= number_channels)
        return;

	const double pixels_offset = offset / scale;
    const double samplerate = _data->get_samplerate();
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
            pixels_offset, samples_per_pixel, number_channels);
	else
		paint_envelope(p, snapshot, y, left,
			start_sample, end_sample,
			pixels_offset, samples_per_pixel);
}

void DsoSignal::paint_trace(QPainter &p,
    const boost::shared_ptr<pv::data::DsoSnapshot> &snapshot,
    int y, int left, const int64_t start, const int64_t end,
    const double pixels_offset, const double samples_per_pixel, uint64_t num_channels)
{
    const int64_t sample_count = end - start;

    if (sample_count > 0) {
        const uint8_t *const samples = snapshot->get_samples(start, end, get_index());
        assert(samples);

        p.setPen(_colour);
        //p.setPen(QPen(_colour, 3, Qt::SolidLine));

        QPointF *points = new QPointF[sample_count];
        QPointF *point = points;

        for (int64_t sample = start; sample < end; sample++) {
            const float x = (sample / samples_per_pixel - pixels_offset) + left;
            uint8_t offset = samples[(sample - start)*num_channels];
            *point++ = QPointF(x,
                               y - offset * _scale);
        }

        p.drawPolyline(points, point - points);

        //delete[] samples;
        delete[] points;
    }
}

void DsoSignal::paint_envelope(QPainter &p,
    const boost::shared_ptr<pv::data::DsoSnapshot> &snapshot,
	int y, int left, const int64_t start, const int64_t end,
	const double pixels_offset, const double samples_per_pixel)
{
	using namespace Qt;
    using pv::data::DsoSnapshot;

    DsoSnapshot::EnvelopeSection e;
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
        const DsoSnapshot::EnvelopeSample *const s =
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

const std::vector< std::pair<uint64_t, bool> > DsoSignal::cur_edges() const
{

}

void DsoSignal::set_decoder(pv::decoder::Decoder *decoder)
{
    (void)decoder;
}

decoder::Decoder *DsoSignal::get_decoder()
{
    return NULL;
}

void DsoSignal::del_decoder()
{
}

} // namespace view
} // namespace pv
