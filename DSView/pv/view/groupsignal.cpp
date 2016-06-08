/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
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

#include "groupsignal.h"
#include "pv/data/group.h"
#include "pv/data/groupsnapshot.h"
#include "view.h"

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
                         std::list<int> probe_index_list, int group_index) :
    Trace(name, probe_index_list, SR_CHANNEL_GROUP, group_index),
    _data(data)
{
    _colour = SignalColours[probe_index_list.front() % countof(SignalColours)];
    _scale = _totalHeight * 1.0f / std::pow(2.0, static_cast<double>(probe_index_list.size()));
}

GroupSignal::~GroupSignal()
{
}

bool GroupSignal::enabled() const
{
    return true;
}

boost::shared_ptr<pv::data::SignalData> GroupSignal::data() const
{
    return _data;
}

void GroupSignal::set_scale(float scale)
{
	_scale = scale;
}

void GroupSignal::paint_mid(QPainter &p, int left, int right)
{
    assert(_data);
    assert(_view);
    assert(right >= left);

    const int y = get_y() + _totalHeight * 0.5;
    const double scale = _view->scale();
    assert(scale > 0);
    const double offset = _view->offset();

    _scale = _totalHeight * 1.0f / std::pow(2.0, static_cast<int>(_index_list.size()));

    const deque< boost::shared_ptr<pv::data::GroupSnapshot> > &snapshots =
		_data->get_snapshots();
    if (snapshots.empty())
		return;

    const boost::shared_ptr<pv::data::GroupSnapshot> &snapshot =
            snapshots.at(_sec_index);

	const double pixels_offset = offset / scale;
    const double samplerate = _data->samplerate();
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

void GroupSignal::paint_type_options(QPainter &p, int right, const QPoint pt)
{
    (void)pt;

    int y = get_y();
    const QRectF group_index_rect = get_rect(CHNLREG, y, right);
    QString index_string;
    int last_index;
    p.setPen(QPen(DARK_FORE, 1, Qt::DashLine));
    p.drawLine(group_index_rect.bottomLeft(), group_index_rect.bottomRight());
    std::list<int>::iterator i = _index_list.begin();
    last_index = (*i);
    index_string = QString::number(last_index);
    while (++i != _index_list.end()) {
        if ((*i) == last_index + 1 && index_string.indexOf("-") < 3 && index_string.indexOf("-") > 0)
            index_string.replace(QString::number(last_index), QString::number((*i)));
        else if ((*i) == last_index + 1)
            index_string = QString::number((*i)) + "-" + index_string;
        else
            index_string = QString::number((*i)) + "," + index_string;
        last_index = (*i);
    }
    p.setPen(DARK_FORE);
    p.drawText(group_index_rect, Qt::AlignRight | Qt::AlignVCenter, index_string);
}

QRectF GroupSignal::get_rect(GroupSetRegions type, int y, int right)
{
    const QSizeF name_size(right - get_leftWidth() - get_rightWidth(), SquareWidth);

    if (type == CHNLREG)
        return QRectF(
            get_leftWidth() + name_size.width() + Margin,
            y - SquareWidth / 2,
            SquareWidth * SquareNum, SquareWidth);
    else
        return QRectF(0, 0, 0, 0);
}

} // namespace view
} // namespace pv
