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

 
#include <math.h>
#include <QTimer>

#include "view.h"
#include "../dsvdef.h"
#include "lissajoustrace.h"
#include "../data/dsosnapshot.h"
#include "../sigsession.h"

#include "../ui/langresource.h"
 
using namespace std;

namespace pv {
namespace view {

LissajousTrace::LissajousTrace(bool enable,
                     data::DsoSnapshot *data,
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
               L_S(STR_PAGE_DLG, S_ID(IDS_DLG_LISSAJOUS_FIGURE), "Lissajous Figure"), Qt::AlignTop | Qt::AlignLeft);

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

        if (_data->empty())
            return;

        int channel_num = _data->get_channel_num();
        if (channel_num < 2){
            p.setPen(view::View::Red);
            p.drawText(_border.marginsRemoved(QMargins(10, 30, 10, 30)),
                       L_S(STR_PAGE_DLG, S_ID(IDS_DLG_CHAN_NUM_ERR2), "Requires the data of two channels."));
            return;
        }

        int left = _border.left();
        int bottom = _border.bottom();
        double scale = _border.width() / 255.0;
        uint64_t sample_count = _data->get_sample_count() * min(_percent / 100.0, 1.0);
        QPointF *points = new QPointF[sample_count];
        QPointF *point = points;

        if (_xIndex >= channel_num || _yIndex >= channel_num) {
            p.setPen(view::View::Red);
            p.drawText(_border.marginsRemoved(QMargins(10, 30, 10, 30)),
                       L_S(STR_PAGE_DLG, S_ID(IDS_DLG_DATA_SOURCE_ERROR), "Data source error."));
        }
        else {
            const uint8_t* chan_data_array[2];
            chan_data_array[_xIndex] = _data->get_samples(0, sample_count-1, _xIndex);
            chan_data_array[_yIndex] = _data->get_samples(0, sample_count-1, _yIndex);

            for (uint64_t i = 0; i < sample_count; i++) {
                const uint8_t* dx = chan_data_array[_xIndex];
                const uint8_t* dy = chan_data_array[_yIndex];

                *point++ = QPointF(left + dx[i] * scale,
                                    bottom - dy[i] * scale);
            }

            p.setPen(view::View::Blue);
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
