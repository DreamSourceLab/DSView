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

#include "xcursor.h"

#include "view.h"
#include "ruler.h"
#include "../device/device.h"
#include "dsosignal.h"

#include <QPainter>

#include <boost/foreach.hpp>

using namespace boost;
using namespace std;

namespace pv {
namespace view {

XCursor::XCursor(View &view, QColor &colour,
    double value0, double value1) :
	_view(view),
    _yvalue(0.5),
    _value0(value0),
    _value1(value1),
    _grabbed(XCur_None),
	_colour(colour)
{
    _dsoSig = NULL;
    const std::vector< boost::shared_ptr<Signal> > sigs(_view.session().get_signals());
    BOOST_FOREACH(const boost::shared_ptr<Signal> s, sigs) {
        boost::shared_ptr<DsoSignal> dsoSig;
        if ((dsoSig = dynamic_pointer_cast<DsoSignal>(s)))
            if (dsoSig->enabled()) {
                _dsoSig = dsoSig;
                break;
            }
    }
}

XCursor::XCursor(const XCursor &x) :
	QObject(),
    _view(x._view),
    _dsoSig(x._dsoSig),
    _yvalue(x._yvalue),
    _value0(x._value0),
    _value1(x._value1),
    _grabbed(XCur_None),
    _colour(x._colour)
{
}

QColor XCursor::colour() const
{
    return _colour;
}

void XCursor::set_colour(QColor color)
{
    _colour = color;
}

/**
 * Gets/Sets the mapping channel of the marker
 */
boost::shared_ptr<DsoSignal> XCursor::channel() const
{
    return _dsoSig;
}
void XCursor::set_channel(boost::shared_ptr<DsoSignal> sig)
{
    _dsoSig = sig;
}

enum XCursor::XCur_type XCursor::grabbed() const
{
    return _grabbed;
}
void XCursor::set_grabbed(XCur_type type, bool grabbed)
{
    if (_grabbed == XCur_None && grabbed)
        _grabbed = type;
    else if (_grabbed == type && !grabbed)
        _grabbed = XCur_None;
}
void XCursor::rel_grabbed()
{
    _grabbed = XCur_None;
}

double XCursor::value(XCur_type type) const
{
    if (type == XCur_Y)
        return _yvalue;
    else if (type == XCur_X0)
        return _value0;
    else if (type == XCur_X1)
        return _value1;
    else
        return 0;
}

void XCursor::set_value(XCur_type type, double value)
{
    if (type == XCur_Y)
        _yvalue = value;
    else if (type == XCur_X0)
        _value0 = value;
    else if (type == XCur_X1)
        _value1 = value;
    value_changed();
}

void XCursor::paint(QPainter &p, const QRect &rect, XCur_type highlight,  int order)
{
    const int arrow = 3;
    const int x = rect.left() + _yvalue * rect.width();
    const int y0 = rect.top() + _value0 * rect.height();
    const int y1 = rect.top() + _value1 * rect.height();
    QColor color = (order == -1) ? _colour : Ruler::CursorColor[order%8];
    const bool hit0 = (_grabbed == XCur_X0) | (_grabbed == XCur_None && (highlight == XCur_X0 || highlight == XCur_All));
    p.setPen(hit0 ? QPen(color.lighter(), 2, Qt::DashLine) : QPen(color, 1, Qt::DashLine));
    p.drawLine(QPoint(0, y0), QPoint(rect.right()-_v0_size.width(), y0));
    const bool hit1 = (_grabbed == XCur_X1) | (_grabbed == XCur_None && (highlight == XCur_X1 || highlight == XCur_All));
    p.setPen(hit1 ? QPen(color.lighter(), 2, Qt::DashLine) : QPen(color, 1, Qt::DashLine));
    p.drawLine(QPoint(0, y1), QPoint(rect.right()-_v1_size.width(), y1));

    if (_dsoSig) {
        if ((_grabbed == XCur_Y) | (_grabbed == XCur_None && (highlight == XCur_Y || highlight == XCur_All)))
            p.setPen(QPen(_dsoSig->get_colour(), 2, Qt::DashLine));
        else
            p.setPen(QPen(_dsoSig->get_colour(), 1, Qt::DashLine));
        p.drawLine(QPoint(x, y0), QPoint(x, y1));
        p.drawLine(QPoint(x-arrow, y0 + ((y0 > y1) ? -arrow : arrow)), QPoint(x, y0));
        p.drawLine(QPoint(x+arrow, y0 + ((y0 > y1) ? -arrow : arrow)), QPoint(x, y0));
        p.drawLine(QPoint(x-arrow, y1 + ((y0 > y1) ? arrow : -arrow)), QPoint(x, y1));
        p.drawLine(QPoint(x+arrow, y1 + ((y0 > y1) ? arrow : -arrow)), QPoint(x, y1));

        QString vol = " Δ:" + _dsoSig->get_voltage(qAbs(y0 - y1), 2, true);
        const QSizeF delta_size = p.boundingRect(rect, Qt::AlignLeft | Qt::AlignTop, vol).size();
        const QRectF delta_rect = QRectF(x, (y0+y1-delta_size.height())/2, delta_size.width(), delta_size.height());
        p.drawText(delta_rect, Qt::AlignCenter, vol);

        vol = _dsoSig->get_voltage((_dsoSig->get_zero_ratio() - _value0)*rect.height(), 2, true);
        _v0_size = p.boundingRect(rect, Qt::AlignLeft | Qt::AlignTop, vol).size();
        p.drawText(QRectF(rect.right()-_v0_size.width(), y0-_v0_size.height()/2, _v0_size.width(), _v0_size.height()),
                   Qt::AlignLeft | Qt::AlignVCenter,
                   vol);

        vol = _dsoSig->get_voltage((_dsoSig->get_zero_ratio() - _value1)*rect.height(), 2, true);
        _v1_size = p.boundingRect(rect, Qt::AlignLeft | Qt::AlignTop, vol).size();
        p.drawText(QRectF(rect.right()-_v1_size.width(), y1-_v1_size.height()/2, _v1_size.width(), _v1_size.height()),
                   Qt::AlignLeft | Qt::AlignVCenter,
                   vol);
    }

    paint_label(p, rect);
}

/**
 * Gets the map label rectangle.
 * @param rect The rectangle of the xcursor area.
 * @return Returns the map label rectangle.
 */
QRect XCursor::get_map_rect(const QRect &rect) const
{
    const int width = 10;
    const int64_t y = rect.top() + _value0 * rect.height() - width/2;
    return QRect(rect.right()+1, y, width, width);
}

/**
 * Gets the close label rectangle.
 * @param rect The rectangle of the xcursor area.
 * @return Returns the close label rectangle.
 */
QRect XCursor::get_close_rect(const QRect &rect) const
{
    const int width = 10;
    const int64_t y = rect.top() + _value1 * rect.height() - width/2;
    return QRect(rect.right()+1, y, width, width);
}

/**
 * Paints the labels to the xcursor.
 * @param p The painter to draw with.
 * @param rect The rectangle of the xcursor area.
 */
void XCursor::paint_label(QPainter &p, const QRect &rect)
{
    if (_dsoSig) {
        QRect map_rect = get_map_rect(rect);
        p.setPen(Qt::NoPen);
        p.setBrush(_dsoSig->get_colour());
        p.drawRect(map_rect);
        p.setPen(Qt::white);
        p.drawText(map_rect, Qt::AlignCenter | Qt::AlignHCenter, QString::number(_dsoSig->get_index()));
    }

    QRect close = get_close_rect(rect);
    p.setPen(Qt::NoPen);
    if (close.contains(QPoint(_view.hover_point().x(), _view.hover_point().y())))
        p.setBrush(View::Red);
    else
        p.setBrush(_dsoSig->get_colour());
    p.drawRect(close);
    p.setPen(Qt::black);
    p.drawLine(close.left() + 2, close.top() + 2, close.right() - 2, close.bottom() - 2);
    p.drawLine(close.left() + 2, close.bottom() - 2, close.right() - 2, close.top() + 2);
}

} // namespace view
} // namespace pv
