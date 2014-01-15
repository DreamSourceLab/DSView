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

#include <QApplication>

#include "signal.h"
#include "view.h"

namespace pv {
namespace view {

const int Signal::SquareWidth = 20;
const int Signal::Margin = 5;

const int Signal::COLOR = 1;
const int Signal::NAME = 2;
const int Signal::POSTRIG = 3;
const int Signal::HIGTRIG = 4;
const int Signal::NEGTRIG = 5;
const int Signal::LOWTRIG = 6;
const int Signal::LABEL = 7;

const QColor Signal::dsBlue = QColor(17, 133, 209,  255);
const QColor Signal::dsYellow = QColor(238, 178, 17, 255);
const QColor Signal::dsRed = QColor(213, 15, 37, 255);
const QColor Signal::dsGreen = QColor(0, 153, 37, 255);
const QColor Signal::dsGray = QColor(0x88, 0x8A, 0x85, 100);
const QColor Signal::dsLightBlue = QColor(17, 133, 209,  150);
const QColor Signal::dsLightRed = QColor(213, 15, 37, 150);

const QPen Signal::SignalAxisPen(QColor(128, 128, 128, 64));

Signal::Signal(QString name, int index, int type, int order) :
    _type(type),
    _order(order),
    _sec_index(0),
    _name(name),
    _v_offset(0),
    _signalHeight(30),
    _selected(false),
    _trig(0)
{
    _index_list.push_back(index);
}

Signal::Signal(QString name, std::list<int> index_list, int type, int order, int sec_index) :
    _type(type),
    _index_list(index_list),
    _order(order),
    _sec_index(sec_index),
	_name(name),
	_v_offset(0),
    _signalHeight(30),
    _selected(false),
    _trig(0)
{
}

int Signal::get_type() const
{
    return _type;
}

int Signal::get_order() const
{
    return _order;
}

void Signal::set_order(int order)
{
    assert(order >= 0);

    _order = order;
}

int Signal::get_leftWidth() const
{
    return SquareWidth + Margin;
}

int Signal::get_rightWidth() const
{
    return 2 * Margin + 4 * SquareWidth + 1.5 * SquareWidth;
}

int Signal::get_headerHeight() const
{
    return SquareWidth;
}

int Signal::get_index() const
{
    return _index_list.front();
}

std::list<int> &Signal::get_index_list()
{
    return _index_list;
}

void Signal::set_index_list(std::list<int> index_list)
{
    assert(index_list.size() != 0);

    _index_list = index_list;
}

int Signal::get_sec_index() const
{
    return _sec_index;
}

void Signal::set_sec_index(int sec_index)
{
    _sec_index = sec_index;
}

QString Signal::get_name() const
{
	return _name;
}

void Signal::set_name(QString name)
{
	_name = name;
}

QColor Signal::get_colour() const
{
	return _colour;
}

void Signal::set_colour(QColor colour)
{
	_colour = colour;
}

int Signal::get_v_offset() const
{
	return _v_offset;
}

void Signal::set_v_offset(int v_offset)
{
	_v_offset = v_offset;
}

int Signal::get_old_v_offset() const
{
    return _old_v_offset;
}

void Signal::set_old_v_offset(int v_offset)
{
    _old_v_offset = v_offset;
}

int Signal::get_signalHeight() const
{
    return _signalHeight;
}

void Signal::set_signalHeight(int height)
{
    _signalHeight = height;
}

bool Signal::selected() const
{
	return _selected;
}

void Signal::select(bool select)
{
	_selected = select;
}

int Signal::get_trig() const
{
    return _trig;
}

void Signal::set_trig(int trig)
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
}

void Signal::paint_label(QPainter &p, int y, int right, bool hover, int action)
{
    compute_text_size(p);
    const QRectF color_rect = get_rect("color", y, right);
    const QRectF name_rect  = get_rect("name",  y, right);
    const QRectF posTrig_rect  = get_rect("posTrig",  y, right);
    const QRectF higTrig_rect  = get_rect("higTrig",  y, right);
    const QRectF negTrig_rect  = get_rect("negTrig",  y, right);
    const QRectF lowTrig_rect  = get_rect("lowTrig",  y, right);
    const QRectF label_rect = get_rect("label", y, right);

    p.setRenderHint(QPainter::Antialiasing);
    // Paint the ColorButton
    p.setPen(Qt::transparent);
    p.setBrush(_colour);
    p.drawRect(color_rect);

    // Paint the signal name
    p.setPen(Qt::black);
    p.drawText(name_rect, Qt::AlignLeft | Qt::AlignVCenter, _name);

    // Paint the trigButton
    if (_type == DS_LOGIC) {
        p.setPen(Qt::transparent);
        p.setBrush(((hover && action == POSTRIG) || (_trig == POSTRIG)) ?
                       dsYellow :
                       dsBlue);
        p.drawRect(posTrig_rect);
        p.setBrush(((hover && action == HIGTRIG) || (_trig == HIGTRIG)) ?
                       dsYellow :
                       dsBlue);
        p.drawRect(higTrig_rect);
        p.setBrush(((hover && action == NEGTRIG) || (_trig == NEGTRIG)) ?
                       dsYellow :
                       dsBlue);
        p.drawRect(negTrig_rect);
        p.setBrush(((hover && action == LOWTRIG) || (_trig == LOWTRIG)) ?
                       dsYellow :
                       dsBlue);
        p.drawRect(lowTrig_rect);

        p.setPen(QPen(Qt::blue, 1, Qt::DotLine));
        p.setBrush(Qt::transparent);
        p.drawLine(posTrig_rect.right(), posTrig_rect.top() + 3,
                   posTrig_rect.right(), posTrig_rect.bottom() - 3);
        p.drawLine(higTrig_rect.right(), higTrig_rect.top() + 3,
                   higTrig_rect.right(), higTrig_rect.bottom() - 3);
        p.drawLine(negTrig_rect.right(), negTrig_rect.top() + 3,
                   negTrig_rect.right(), negTrig_rect.bottom() - 3);

        p.setPen(QPen(Qt::white, 2, Qt::SolidLine));
        p.setBrush(Qt::transparent);
        p.drawLine(posTrig_rect.left() + 5, posTrig_rect.bottom() - 5,
                   posTrig_rect.center().x(), posTrig_rect.bottom() - 5);
        p.drawLine(posTrig_rect.center().x(), posTrig_rect.bottom() - 5,
                   posTrig_rect.center().x(), posTrig_rect.top() + 5);
        p.drawLine(posTrig_rect.center().x(), posTrig_rect.top() + 5,
                   posTrig_rect.right() - 5, posTrig_rect.top() + 5);

        p.drawLine(higTrig_rect.left() + 5, higTrig_rect.top() + 5,
                   higTrig_rect.right() - 5, higTrig_rect.top() + 5);

        p.drawLine(negTrig_rect.left() + 5, negTrig_rect.top() + 5,
                   negTrig_rect.center().x(), negTrig_rect.top() + 5);
        p.drawLine(negTrig_rect.center().x(), negTrig_rect.top() + 5,
                   negTrig_rect.center().x(), negTrig_rect.bottom() - 5);
        p.drawLine(negTrig_rect.center().x(), negTrig_rect.bottom() - 5,
                   negTrig_rect.right() - 5, negTrig_rect.bottom() - 5);

        p.drawLine(lowTrig_rect.left() + 5, lowTrig_rect.bottom() - 5,
                   lowTrig_rect.right() - 5, lowTrig_rect.bottom() - 5);
    } else if (_type == DS_GROUP || _type == DS_PROTOCOL) {
        const QRectF group_index_rect = get_rect("groupIndex", y, right);
        QString index_string;
        int last_index;
        p.setPen(Qt::transparent);
        p.setBrush(dsBlue);
        p.drawRect(group_index_rect);
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
        p.setPen(Qt::white);
        p.drawText(group_index_rect, Qt::AlignRight | Qt::AlignVCenter, index_string);
    }

	// Paint the label
    const QPointF points[] = {
        label_rect.topLeft(),
        label_rect.topRight(),
        QPointF(right, y),
        label_rect.bottomRight(),
        label_rect.bottomLeft()
    };

	p.setPen(Qt::transparent);
    p.setBrush(((hover && action == LABEL) || _selected) ? dsYellow : dsBlue);
	p.drawPolygon(points, countof(points));

    if ((hover && action == LABEL) || _selected) {
        p.setPen(QPen(dsBlue, 2, Qt::DotLine));
        p.setBrush(Qt::transparent);
        p.drawPoint(label_rect.right(), label_rect.top() + 4);
        p.drawPoint(label_rect.right(), label_rect.top() + 8);
        p.drawPoint(label_rect.right(), label_rect.top() + 12);
        p.drawPoint(label_rect.right(), label_rect.top() + 16);
    }

	// Paint the text
    p.setPen(Qt::white);
    if (_type == DS_GROUP)
        p.drawText(label_rect, Qt::AlignCenter | Qt::AlignVCenter, "G");
    else if (_type == DS_ANALOG)
        p.drawText(label_rect, Qt::AlignCenter | Qt::AlignVCenter, "A");
    else if (_type == DS_PROTOCOL)
        p.drawText(label_rect, Qt::AlignCenter | Qt::AlignVCenter, "D");
    else
        p.drawText(label_rect, Qt::AlignCenter | Qt::AlignVCenter, QString::number(_index_list.front()));
}

int Signal::pt_in_rect(int y, int right, const QPoint &point)
{
    const QRectF color = get_rect("color", y, right);
    const QRectF name  = get_rect("name", y, right);
    const QRectF posTrig = get_rect("posTrig", y, right);
    const QRectF higTrig = get_rect("higTrig", y, right);
    const QRectF negTrig = get_rect("negTrig", y, right);
    const QRectF lowTrig = get_rect("lowTrig", y, right);
    const QRectF label = get_rect("label", y, right);

    if (color.contains(point))
        return COLOR;
    else if (name.contains(point))
        return NAME;
    else if (posTrig.contains(point) && _type == DS_LOGIC)
        return POSTRIG;
    else if (higTrig.contains(point) && _type == DS_LOGIC)
        return HIGTRIG;
    else if (negTrig.contains(point) && _type == DS_LOGIC)
        return NEGTRIG;
    else if (lowTrig.contains(point) && _type == DS_LOGIC)
        return LOWTRIG;
    else if (label.contains(point))
        return LABEL;
    else
        return 0;
}

void Signal::paint_axis(QPainter &p, int y, int left, int right)
{
	p.setPen(SignalAxisPen);
	p.drawLine(QPointF(left, y + 0.5f), QPointF(right, y + 0.5f));
}

void Signal::compute_text_size(QPainter &p)
{
	_text_size = QSize(
        p.boundingRect(QRectF(), 0, "99").width(),
        p.boundingRect(QRectF(), 0, "99").height());
}

QRectF Signal::get_rect(const char *s, int y, int right)
{
    const QSizeF color_size(SquareWidth, SquareWidth);
    const QSizeF name_size(right - get_leftWidth() - get_rightWidth(), SquareWidth);
    const QSizeF label_size(_text_size.width() + Margin, SquareWidth);

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
    else if (!strcmp(s, "posTrig"))
        return QRectF(
            get_leftWidth() + name_size.width() + Margin,
            y - color_size.height() / 2,
            color_size.width(), color_size.height());
    else if (!strcmp(s, "higTrig"))
        return QRectF(
            get_leftWidth() + name_size.width() + SquareWidth + Margin,
            y - color_size.height() / 2,
            color_size.width(), color_size.height());
    else if (!strcmp(s, "negTrig"))
        return QRectF(
            get_leftWidth() + name_size.width() + 2 * SquareWidth + Margin,
            y - color_size.height() / 2,
            color_size.width(), color_size.height());
    else if (!strcmp(s, "lowTrig"))
        return QRectF(
            get_leftWidth() + name_size.width() + 3 * SquareWidth + Margin,
            y - color_size.height() / 2,
            color_size.width(), color_size.height());
    else if (!strcmp(s, "groupIndex"))
        return QRectF(
            get_leftWidth() + name_size.width() + Margin,
            y - color_size.height() / 2,
            color_size.width() * 4, color_size.height());
    else
        return QRectF(
            2,
            y - color_size.height() / 2,
            color_size.width(), color_size.height());
}

} // namespace view
} // namespace pv
