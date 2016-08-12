/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
 * Copyright (C) 2013 Joel Holdsworth <joel@airwebreathe.org.uk>
 * Copyright (C) 2014 DreamSourceLab <support@dreamsourcelab.com>
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

#include <assert.h>
#include <math.h>

#include <QApplication>
#include <QFormLayout>
#include <QLineEdit>

#include "trace.h"
#include "view.h"
#include "../device/devinst.h"
#include "../sigsession.h"

namespace pv {
namespace view {

const QColor Trace::dsBlue = QColor(17, 133, 209,  255);
const QColor Trace::dsYellow = QColor(238, 178, 17, 255);
const QColor Trace::dsRed = QColor(213, 15, 37, 255);
const QColor Trace::dsGreen = QColor(0, 153, 37, 200);
const QColor Trace::dsGray = QColor(0x88, 0x8A, 0x85, 60);
const QColor Trace::dsFore = QColor(0xff, 0xff, 0xff, 60);
const QColor Trace::dsBack = QColor(0x16, 0x18, 0x23, 200);
const QColor Trace::dsDisable = QColor(0x88, 0x8A, 0x85, 200);
const QColor Trace::dsActive = QColor(17, 133, 209, 255);
const QColor Trace::dsLightBlue = QColor(17, 133, 209,  150);
const QColor Trace::dsLightRed = QColor(213, 15, 37, 150);
const QPen Trace::SignalAxisPen = QColor(128, 128, 128, 64);

const QColor Trace::DARK_BACK = QColor(48, 47, 47, 255);
const QColor Trace::DARK_FORE = QColor(150, 150, 150, 255);
const QColor Trace::DARK_HIGHLIGHT = QColor(32, 32, 32, 255);
const QColor Trace::DARK_BLUE = QColor(17, 133, 209,  255);

const QColor Trace::PROBE_COLORS[8] = {
    QColor(0x50, 0x50, 0x50),	// Black
    QColor(0x8F, 0x52, 0x02),	// Brown
    QColor(0xCC, 0x00, 0x00),	// Red
    QColor(0xF5, 0x79, 0x00),	// Orange
    QColor(0xED, 0xD4, 0x00),	// Yellow
    QColor(0x73, 0xD2, 0x16),	// Green
    QColor(0x34, 0x65, 0xA4),	// Blue
    QColor(0x75, 0x50, 0x7B),	// Violet
//    QColor(17, 133, 209),
//    QColor(17, 133, 209),
//    QColor(17, 133, 209),
//    QColor(17, 133, 209),
//    QColor(17, 133, 209),
//    QColor(17, 133, 209),
//    QColor(17, 133, 209),
//    QColor(17, 133, 209),
};

const QPen Trace::AxisPen(QColor(128, 128, 128, 64));
const int Trace::LabelHitPadding = 2;

Trace::Trace(QString name, uint16_t index, int type) :
    _view(NULL),
	_name(name),
    _v_offset(INT_MAX),
    _type(type),
    _sec_index(0),
    _totalHeight(30),
    _typeWidth(SquareNum)
{
    _index_list.push_back(index);
}

Trace::Trace(QString name, std::list<int> index_list, int type, int sec_index) :
    _view(NULL),
    _name(name),
    _v_offset(INT_MAX),
    _type(type),
    _index_list(index_list),
    _sec_index(sec_index),
    _totalHeight(30),
    _typeWidth(SquareNum)
{
}

Trace::Trace(const Trace &t) :
    _view(t._view),
    _name(t._name),
    _colour(t._colour),
    _v_offset(t._v_offset),
    _type(t._type),
    _index_list(t._index_list),
    _sec_index(t._sec_index),
    _old_v_offset(t._old_v_offset),
    _totalHeight(t._totalHeight),
    _typeWidth(t._typeWidth),
    _text_size(t._text_size)
{
}

QString Trace::get_name() const
{
	return _name;
}

void Trace::set_name(QString name)
{
	_name = name;
}

QColor Trace::get_colour() const
{
	return _colour;
}

void Trace::set_colour(QColor colour)
{
	_colour = colour;
}

int Trace::get_v_offset() const
{
	return _v_offset;
}

void Trace::set_v_offset(int v_offset)
{
	_v_offset = v_offset;
}


int Trace::get_type() const
{
    return _type;
}

int Trace::get_index() const
{
    return _index_list.front();
}

std::list<int> &Trace::get_index_list()
{
    return _index_list;
}

void Trace::set_index_list(std::list<int> index_list)
{
    assert(index_list.size() != 0);

    _index_list = index_list;
}

int Trace::get_sec_index() const
{
    return _sec_index;
}

void Trace::set_sec_index(int sec_index)
{
    _sec_index = sec_index;
}

int Trace::get_old_v_offset() const
{
    return _old_v_offset;
}

void Trace::set_old_v_offset(int v_offset)
{
    _old_v_offset = v_offset;
}

int Trace::get_zero_vpos()
{
    return _v_offset;
}

int Trace::get_totalHeight() const
{
    return _totalHeight;
}

void Trace::set_totalHeight(int height)
{
    _totalHeight = height;
}

void Trace::set_view(pv::view::View *view)
{
	assert(view);
	_view = view;
}

pv::view::View* Trace::get_view() const
{
    return _view;
}

void Trace::set_viewport(pv::view::Viewport *viewport)
{
    assert(viewport);
    _viewport = viewport;
}

pv::view::Viewport* Trace::get_viewport() const
{
    return _viewport;
}

void Trace::paint_back(QPainter &p, int left, int right)
{
    QPen pen(Signal::dsGray);
    pen.setStyle(Qt::DotLine);
    p.setPen(pen);
    const double sigY = get_y();
    p.drawLine(left, sigY, right, sigY);
}

void Trace::paint_mid(QPainter &p, int left, int right)
{
	(void)p;
	(void)left;
	(void)right;
}

void Trace::paint_fore(QPainter &p, int left, int right)
{
	(void)p;
	(void)left;
	(void)right;
}

void Trace::paint_label(QPainter &p, int right, const QPoint pt)
{
    if (_type == SR_CHANNEL_FFT && !enabled())
        return;

    compute_text_size(p);
    const int y = get_y();

    const QRectF color_rect = get_rect("color", y, right);
    const QRectF name_rect  = get_rect("name",  y, right);
    const QRectF label_rect = get_rect("label", get_zero_vpos(), right);

    //p.setRenderHint(QPainter::Antialiasing);
    // Paint the ColorButton
    p.setPen(Qt::transparent);
    p.setBrush(enabled() ? _colour : dsDisable);
    p.drawRect(color_rect);

    // Paint the signal name
    p.setPen(enabled() ?  DARK_FORE: dsDisable);
    p.drawText(name_rect, Qt::AlignLeft | Qt::AlignVCenter, _name);

    // Paint the trigButton
    paint_type_options(p, right, pt);

    // Paint the label
    if (enabled()) {
        const QPointF points[] = {
            label_rect.topLeft(),
            label_rect.topRight(),
            QPointF(right, get_zero_vpos()),
            label_rect.bottomRight(),
            label_rect.bottomLeft()
        };

        p.setPen(Qt::transparent);
        if (_type == SR_CHANNEL_DSO || _type == SR_CHANNEL_FFT) {
            p.setBrush((label_rect.contains(pt) || selected()) ? _colour.darker() : _colour);
            p.drawPolygon(points, countof(points));
        } else {
            QColor color = PROBE_COLORS[*_index_list.begin() % countof(PROBE_COLORS)];
            p.setBrush((label_rect.contains(pt) || selected()) ? color.lighter() : color);
            p.drawPolygon(points, countof(points));
        }

        // Paint the text
        p.setPen(Qt::white);
        if (_type == SR_CHANNEL_GROUP)
            p.drawText(label_rect, Qt::AlignCenter | Qt::AlignVCenter, "G");
        else if (_type == SR_CHANNEL_ANALOG)
            p.drawText(label_rect, Qt::AlignCenter | Qt::AlignVCenter, "A");
        else if (_type == SR_CHANNEL_DECODER)
            p.drawText(label_rect, Qt::AlignCenter | Qt::AlignVCenter, "D");
        else if (_type == SR_CHANNEL_FFT)
            p.drawText(label_rect, Qt::AlignCenter | Qt::AlignVCenter, "M");
        else
            p.drawText(label_rect, Qt::AlignCenter | Qt::AlignVCenter, QString::number(_index_list.front()));
    }
}

void Trace::paint_type_options(QPainter &p, int right, const QPoint pt)
{
    (void)p;
    (void)right;
    (void)pt;
}

bool Trace::mouse_double_click(int right, const QPoint pt)
{
    (void)right;
    (void)pt;
    return false;
}

bool Trace::mouse_press(int right, const QPoint pt)
{
    (void)right;
    (void)pt;
    return false;
}

bool Trace::mouse_wheel(int right, const QPoint pt, const int shift)
{
    (void)right;
    (void)pt;
    (void)shift;
    return false;
}

int Trace::pt_in_rect(int y, int right, const QPoint &point)
{
    const QRectF color = get_rect("color", y, right);
    const QRectF name  = get_rect("name", y, right);
    const QRectF label = get_rect("label", get_zero_vpos(), right);

    if (color.contains(point) && enabled())
        return COLOR;
    else if (name.contains(point) && enabled())
        return NAME;
    else if (label.contains(point) && enabled())
        return LABEL;
    else
        return 0;
}

void Trace::paint_axis(QPainter &p, int y, int left, int right)
{
    p.setPen(SignalAxisPen);
    p.drawLine(QPointF(left, y + 0.5f), QPointF(right, y + 0.5f));
}

void Trace::compute_text_size(QPainter &p)
{
    _text_size = QSize(
        p.boundingRect(QRectF(), 0, "99").width(),
        p.boundingRect(QRectF(), 0, "99").height());
}

QRect Trace::get_view_rect() const
{
    assert(_view);
    return QRect(0, 0, _view->viewport()->width(), _view->viewport()->height());
}

int Trace::get_y() const
{
    return _v_offset;
}

QColor Trace::get_text_colour() const
{
	return (_colour.lightness() > 64) ? Qt::black : Qt::white;
}

void Trace::on_text_changed(const QString &text)
{
	set_name(text);
	text_changed();
}

void Trace::on_colour_changed(const QColor &colour)
{
	set_colour(colour);
	colour_changed();
}

int Trace::rows_size()
{
    return 1;
}

int Trace::get_leftWidth() const
{
    return SquareWidth/2 + Margin;
}

int Trace::get_rightWidth() const
{
    return 2 * Margin + _typeWidth * SquareWidth + 1.5 * SquareWidth;
}

int Trace::get_headerHeight() const
{
    return SquareWidth;
}

QRectF Trace::get_rect(const char *s, int y, int right) const
{
    const QSizeF color_size(get_leftWidth() - Margin, SquareWidth);
    const QSizeF name_size(right - get_leftWidth() - get_rightWidth(), SquareWidth);
    //const QSizeF label_size(_text_size.width() + Margin, SquareWidth);
    const QSizeF label_size(SquareWidth, SquareWidth);

    if (!strcmp(s, "name"))
        return QRectF(
            get_leftWidth(),
            y - name_size.height() / 2,
            name_size.width(), name_size.height());
    else if (!strcmp(s, "label"))
        return QRectF(
            right - 1.5f * label_size.width(),
            y - label_size.height() / 2,
            label_size.width(), label_size.height());
    else if (!strcmp(s, "color"))
        return QRectF(
            2,
            y - color_size.height() / 2,
            color_size.width(), color_size.height());
    else
        return QRectF(
            2,
            y - SquareWidth / 2,
            SquareWidth, SquareWidth);
}

} // namespace view
} // namespace pv
