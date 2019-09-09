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

#include "../../extdef.h"
#include "lissajoustrace.h"
#include "pv/data/dso.h"
#include "pv/data/dsosnapshot.h"
#include "view.h"
#include "../sigsession.h"
#include "../device/devinst.h"

#include <boost/foreach.hpp>

#include <QDebug>
#include <QTimer>

using namespace boost;
using namespace std;

namespace pv {
namespace view {

LissajousTrace::LissajousTrace(bool enable,
                     boost::shared_ptr<data::Dso> data,
                     int xIndex, int yIndex, int percent):
    Trace("Lissajous", xIndex, SR_CHANNEL_LISSAJOUS),
    _data(data),
    _enable(enable),
    _xIndex(xIndex),
    _yIndex(yIndex),
    _percent(percent)
{

}

LissajousTrace::~LissajousTrace()
{
}

bool LissajousTrace::enabled() const
{
    return _enable;
}

void LissajousTrace::set_enable(bool enable)
{
    _enable = enable;
}

int LissajousTrace::xIndex() const
{
    return _xIndex;
}

int LissajousTrace::yIndex() const
{
    return _yIndex;
}

int LissajousTrace::percent() const
{
    return _percent;
}

boost::shared_ptr<pv::data::Dso> LissajousTrace::get_data() const
{
    return _data;
}

void LissajousTrace::set_data(boost::shared_ptr<data::Dso> data)
{
    _data = data;
}

int LissajousTrace::rows_size()
{
    return 0;
}

void LissajousTrace::paint_back(QPainter &p, int left, int right, QColor fore, QColor back)
{
    assert(_view);

    fore.setAlpha(view::View::BackAlpha);
    const int height = _viewport->height();
    const int width = right - left;
    const int square = min(width, height);
    const QPoint leftTop = QPoint(width > square ? (width-square)/2 : 0,
                                  height > square ? (height-square)/2 : 0);
    _border = QRect(leftTop.x(), leftTop.y(), square, square);

    QPen solidPen(fore);
    solidPen.setStyle(Qt::SolidLine);
    p.setPen(solidPen);
    p.setBrush(back.black() > 0x80 ? back.darker() : back.lighter());
    p.drawRect(_border);

    QPen dashPen(fore);
    dashPen.setStyle(Qt::DashLine);
    p.setPen(dashPen);

    const double spanY =square / DIV_NUM;
    for (int i = 1; i < DIV_NUM; i++) {
        const double posY = _border.top() + spanY * i;
        p.drawLine(_border.left(), posY, _border.right(), posY);
    }
    const double spanX = square / DIV_NUM;
    for (int i = 1; i < DIV_NUM; i++) {
        const double posX = _border.left() + spanX * i;
        p.drawLine(posX, _border.top(), posX, _border.bottom());
    }

    fore.setAlpha(view::View::ForeAlpha);
    p.setPen(fore);
    p.drawText(_border.marginsRemoved(QMargins(10, 10, 10, 10)),
               tr("Lissajous Figure"), Qt::AlignTop | Qt::AlignLeft);

    _view->set_back(true);
}

void LissajousTrace::paint_mid(QPainter &p, int left, int right, QColor fore, QColor back)
{
    (void)fore;
    (void)back;
    (void)left;
    (void)right;

    assert(_data);
    assert(_view);
    assert(right >= left);

    if (enabled()) {
        const deque< boost::shared_ptr<pv::data::DsoSnapshot> > &snapshots =
            _data->get_snapshots();
        if (snapshots.empty())
            return;
        const boost::shared_ptr<pv::data::DsoSnapshot> &snapshot =
            snapshots.front();
        if (snapshot->empty())
            return;

        int left = _border.left();
        int bottom = _border.bottom();
        double scale = _border.width() / 255.0;
        uint64_t sample_count = snapshot->get_sample_count() * min(_percent / 100.0, 1.0);
        QPointF *points = new QPointF[sample_count];
        QPointF *point = points;

        int channel_num = snapshot->get_channel_num();
        if (_xIndex >= channel_num || _yIndex >= channel_num) {
            p.setPen(view::View::Red);
            p.drawText(_border.marginsRemoved(QMargins(10, 30, 10, 30)),
                       tr("Data source error."));
        } else {
            const uint8_t *const samples = snapshot->get_samples(0, sample_count-1, 0);

            for (uint64_t i = 0; i < sample_count; i++) {
                *point++ = QPointF(left + samples[i*channel_num + _xIndex] * scale,
                                   bottom - samples[i*channel_num + _yIndex] * scale);
            }
            p.setPen(view::View::Blue);
            //p.drawPoints(points, sample_count);
            p.drawPolyline(points, point - points);
            delete[] points;
        }
    }
}

void LissajousTrace::paint_fore(QPainter &p, int left, int right, QColor fore, QColor back)
{
    (void)p;
    (void)left;
    (void)right;
    (void)fore;
    (void)back;

    assert(_view);
}

void LissajousTrace::paint_label(QPainter &p, int right, const QPoint pt, QColor fore)
{
    (void)p;
    (void)right;
    (void)pt;
    (void)fore;
}

} // namespace view
} // namespace pv
