/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
 * Copyright (C) 2013 Joel Holdsworth <joel@airwebreathe.org.uk>
 * Copyright (C) 2014 DreamSourceLab <dreamsourcelab@dreamsourcelab.com>
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
const QColor Trace::dsFore = QColor(0xff, 0xff, 0xff, 100);
const QColor Trace::dsBack = QColor(0x16, 0x18, 0x23, 180);
const QColor Trace::dsDisable = QColor(0x88, 0x8A, 0x85, 200);
const QColor Trace::dsActive = QColor(17, 133, 209, 255);
const QColor Trace::dsLightBlue = QColor(17, 133, 209,  150);
const QColor Trace::dsLightRed = QColor(213, 15, 37, 150);
const QPen Trace::SignalAxisPen = QColor(128, 128, 128, 64);

const QPen Trace::AxisPen(QColor(128, 128, 128, 64));
const int Trace::LabelHitPadding = 2;

Trace::Trace(QString name, int index, int type) :
    _view(NULL),
	_name(name),
    _v_offset(INT_MAX),
    _type(type),
    _sec_index(0),
    _signalHeight(30),
    _trig(0)
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
    _signalHeight(30),
    _trig(0)
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
    _signalHeight(t._signalHeight),
    _trig(t._trig),
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

int Trace::get_zeroPos()
{
    return _v_offset - _view->v_offset();
}

int Trace::get_signalHeight() const
{
    return _signalHeight;
}

void Trace::set_signalHeight(int height)
{
    _signalHeight = height;
}

int Trace::get_trig() const
{
    return _trig;
}

void Trace::set_trig(int trig)
{
    _trig = trig;
    if (trig == 0)
        ds_trigger_probe_set(_index_list.front(), 'X', 'X');
    else if (trig == POSTRIG)
        ds_trigger_probe_set(_index_list.front(), 'R', 'X');
    else if (trig == HIGTRIG)
        ds_trigger_probe_set(_index_list.front(), '1', 'X');
    else if (trig == NEGTRIG)
        ds_trigger_probe_set(_index_list.front(), 'F', 'X');
    else if (trig == LOWTRIG)
        ds_trigger_probe_set(_index_list.front(), '0', 'X');
    else if (trig == EDGETRIG)
        ds_trigger_probe_set(_index_list.front(), 'C', 'X');
}

void Trace::set_view(pv::view::View *view)
{
	assert(view);
	_view = view;
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

void Trace::paint_label(QPainter &p, int right, bool hover, int action)
{
    compute_text_size(p);
    const int y = get_y();

    const QRectF color_rect = get_rect("color", y, right);
    const QRectF name_rect  = get_rect("name",  y, right);
    const QRectF label_rect = get_rect("label", get_zeroPos(), right);

    p.setRenderHint(QPainter::Antialiasing);
    // Paint the ColorButton
    p.setPen(Qt::transparent);
    p.setBrush(enabled() ? _colour : dsDisable);
    p.drawRect(color_rect);

    // Paint the signal name
    p.setPen(enabled() ? Qt::black : dsDisable);
    p.drawText(name_rect, Qt::AlignLeft | Qt::AlignVCenter, _name);

    // Paint the trigButton
    paint_type_options(p, right, hover, action);

    // Paint the label
    if (enabled()) {
        const QPointF points[] = {
            label_rect.topLeft(),
            label_rect.topRight(),
            QPointF(right, get_zeroPos()),
            label_rect.bottomRight(),
            label_rect.bottomLeft()
        };

        p.setPen(Qt::transparent);
        if (_type == SR_CHANNEL_DSO)
            p.setBrush(((hover && action == LABEL) || selected()) ? _colour.darker() : _colour);
        else
            p.setBrush(((hover && action == LABEL) || selected()) ? dsYellow : dsBlue);
        p.drawPolygon(points, countof(points));

        p.setPen(QPen(Qt::blue, 1, Qt::DotLine));
        p.setBrush(Qt::transparent);
        p.drawLine(label_rect.right(), label_rect.top() + 3,
                    label_rect.right(), label_rect.bottom() - 3);

        // Paint the text
        p.setPen(Qt::white);
        if (_type == SR_CHANNEL_GROUP)
            p.drawText(label_rect, Qt::AlignCenter | Qt::AlignVCenter, "G");
        else if (_type == SR_CHANNEL_ANALOG)
            p.drawText(label_rect, Qt::AlignCenter | Qt::AlignVCenter, "A");
        else if (_type == SR_CHANNEL_DECODER)
            p.drawText(label_rect, Qt::AlignCenter | Qt::AlignVCenter, "D");
        else
            p.drawText(label_rect, Qt::AlignCenter | Qt::AlignVCenter, QString::number(_index_list.front()));
    }
}

void Trace::paint_type_options(QPainter &p, int right, bool hover, int action)
{
    (void)p;
    (void)right;
    (void)hover;
    (void)action;
}

int Trace::pt_in_rect(int y, int right, const QPoint &point)
{
    const QRectF color = get_rect("color", y, right);
    const QRectF name  = get_rect("name", y, right);
    const QRectF posTrig = get_rect("posTrig", y, right);
    const QRectF higTrig = get_rect("higTrig", y, right);
    const QRectF negTrig = get_rect("negTrig", y, right);
    const QRectF lowTrig = get_rect("lowTrig", y, right);
    const QRectF edgeTrig = get_rect("edgeTrig", y, right);
    const QRectF label = get_rect("label", get_zeroPos(), right);
    const QRectF vDial = get_rect("vDial", y, right);
    const QRectF x1 = get_rect("x1", y, right);
    const QRectF x10 = get_rect("x10", y, right);
    const QRectF x100 = get_rect("x100", y, right);
    const QRectF hDial = get_rect("hDial", y, right);
    const QRectF chEn = get_rect("chEn", y, right);
    const QRectF acdc = get_rect("acdc", y, right);
    const QRectF dsoTrig = get_rect("dsoTrig", 0, right);

    if (color.contains(point) && enabled())
        return COLOR;
    else if (name.contains(point) && enabled())
        return NAME;
    else if (posTrig.contains(point) && _type == SR_CHANNEL_LOGIC)
        return POSTRIG;
    else if (higTrig.contains(point) && _type == SR_CHANNEL_LOGIC)
        return HIGTRIG;
    else if (negTrig.contains(point) && _type == SR_CHANNEL_LOGIC)
        return NEGTRIG;
    else if (lowTrig.contains(point) && _type == SR_CHANNEL_LOGIC)
        return LOWTRIG;
    else if (edgeTrig.contains(point) && _type == SR_CHANNEL_LOGIC)
        return EDGETRIG;
    else if (label.contains(point) && enabled())
        return LABEL;
    else if (vDial.contains(point) && _type == SR_CHANNEL_DSO && enabled())
        return VDIAL;
    else if (x1.contains(point) && _type == SR_CHANNEL_DSO && enabled())
        return X1;
    else if (x10.contains(point) && _type == SR_CHANNEL_DSO && enabled())
        return X10;
    else if (x100.contains(point) && _type == SR_CHANNEL_DSO && enabled())
        return X100;
    else if (hDial.contains(point) && _type == SR_CHANNEL_DSO && enabled())
        return HDIAL;
    else if (chEn.contains(point) && _type == SR_CHANNEL_DSO)
        return CHEN;
    else if (acdc.contains(point) && _type == SR_CHANNEL_DSO && enabled())
        return ACDC;
    else if (dsoTrig.contains(point) && _type == SR_CHANNEL_DSO && enabled())
        return DSOTRIG;
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

QRectF Trace::get_rect(const char *s, int y, int right)
{
    const QSizeF color_size(SquareWidth, SquareWidth);
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
            y - SquareWidth / 2,
            label_size.width(), label_size.height());
    else if (!strcmp(s, "posTrig"))
        return QRectF(
            get_leftWidth() + name_size.width() + Margin,
            y - SquareWidth / 2,
            SquareWidth, SquareWidth);
    else if (!strcmp(s, "higTrig"))
        return QRectF(
            get_leftWidth() + name_size.width() + SquareWidth + Margin,
            y - SquareWidth / 2,
            SquareWidth, SquareWidth);
    else if (!strcmp(s, "negTrig"))
        return QRectF(
            get_leftWidth() + name_size.width() + 2 * SquareWidth + Margin,
            y - SquareWidth / 2,
            SquareWidth, SquareWidth);
    else if (!strcmp(s, "lowTrig"))
        return QRectF(
            get_leftWidth() + name_size.width() + 3 * SquareWidth + Margin,
            y - SquareWidth / 2,
            SquareWidth, SquareWidth);
    else if (!strcmp(s, "edgeTrig"))
        return QRectF(
            get_leftWidth() + name_size.width() + 4 * SquareWidth + Margin,
            y - SquareWidth / 2,
            SquareWidth, SquareWidth);
    else if (!strcmp(s, "groupIndex"))
        return QRectF(
            get_leftWidth() + name_size.width() + Margin,
            y - SquareWidth / 2,
            SquareWidth * SquareNum, SquareWidth);
    else if (!strcmp(s, "vDial"))
        return QRectF(
            get_leftWidth() + name_size.width() + SquareWidth*0.5 + Margin,
            y - SquareWidth * SquareNum,
            SquareWidth * (SquareNum-1), SquareWidth * (SquareNum-1));
    else if (!strcmp(s, "x1"))
        return QRectF(
            get_leftWidth() + name_size.width() + SquareWidth*0.5 + Margin - 45,
            y - SquareWidth - SquareWidth * (SquareNum-1) * 0.85,
            SquareWidth * 1.75, SquareWidth);
    else if (!strcmp(s, "x10"))
        return QRectF(
            get_leftWidth() + name_size.width() + SquareWidth*0.5 + Margin - 45,
            y - SquareWidth - SquareWidth * (SquareNum-1) * 0.55,
            SquareWidth * 1.75, SquareWidth);
    else if (!strcmp(s, "x100"))
        return QRectF(
            get_leftWidth() + name_size.width() + SquareWidth*0.5 + Margin - 45,
            y - SquareWidth - SquareWidth * (SquareNum-1) * 0.25,
            SquareWidth * 1.75, SquareWidth);
    else if (!strcmp(s, "hDial"))
        return QRectF(
            get_leftWidth() + name_size.width() + SquareWidth*0.5 + Margin,
            y + SquareWidth * 1.5,
            SquareWidth * (SquareNum-1), SquareWidth * (SquareNum-1));
    else if (!strcmp(s, "chEn"))
        return QRectF(
            get_leftWidth() + name_size.width() + SquareWidth*0.75 + Margin,
            y - SquareWidth / 2,
            SquareWidth * 1.5, SquareWidth);
    else if (!strcmp(s, "acdc"))
        return QRectF(
            get_leftWidth() + name_size.width() + SquareWidth*2.75 + Margin,
            y - SquareWidth / 2,
            SquareWidth * 1.5, SquareWidth);
    else
        return QRectF(
            2,
            y - SquareWidth / 2,
            SquareWidth, SquareWidth);
}

QRectF Trace::get_view_rect() const
{
    assert(_view);
    return QRectF(0, 0, _view->viewport()->width(), _view->viewport()->height());
}

int Trace::get_y() const
{
	return _v_offset - _view->v_offset();
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
    return SquareWidth + Margin;
}

int Trace::get_rightWidth() const
{
    return 2 * Margin + SquareNum * SquareWidth + 1.5 * SquareWidth;
}

int Trace::get_headerHeight() const
{
    return SquareWidth;
}

} // namespace view
} // namespace pv
