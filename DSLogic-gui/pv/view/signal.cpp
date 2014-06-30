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

const QColor Signal::dsBlue = QColor(17, 133, 209,  255);
const QColor Signal::dsYellow = QColor(238, 178, 17, 255);
const QColor Signal::dsRed = QColor(213, 15, 37, 255);
const QColor Signal::dsGreen = QColor(0, 153, 37, 255);
const QColor Signal::dsGray = QColor(0x88, 0x8A, 0x85, 100);
const QColor Signal::dsDisable = QColor(0x88, 0x8A, 0x85, 200);
const QColor Signal::dsActive = QColor(17, 133, 209, 255);
const QColor Signal::dsLightBlue = QColor(17, 133, 209,  150);
const QColor Signal::dsLightRed = QColor(213, 15, 37, 150);
const QPen Signal::SignalAxisPen = QColor(128, 128, 128, 64);

const quint64 Signal::vDialValue[Signal::vDialValueCount] = {
    5,
    10,
    20,
    50,
    100,
    200,
    500,
    1000,
    2000,
    5000,
};
const QString Signal::vDialUnit[Signal::vDialUnitCount] = {
    "mv",
    "v",
};

const quint64 Signal::hDialValue[Signal::hDialValueCount] = {
    10,
    20,
    50,
    100,
    200,
    500,
    1000,
    2000,
    5000,
    10000,
    20000,
    50000,
    100000,
    200000,
    500000,
    1000000,
    2000000,
    5000000,
    10000000,
    20000000,
    50000000,
    100000000,
    200000000,
    500000000,
    1000000000,
};
const QString Signal::hDialUnit[Signal::hDialUnitCount] = {
    "ns",
    "us",
    "ms",
    "s",
};

Signal::Signal(QString name, int index, int type, int order) :
    _type(type),
    _order(order),
    _sec_index(0),
    _name(name),
    _v_offset(0),
    _signalHeight(30),
    _selected(false),
    _trig(0),
    _vDialActive(false),
    _hDialActive(false),
    _acCoupling(false),
    _active(true),
    _windowHeight(0)
{
    _index_list.push_back(index);
    if (_type == DS_DSO) {
	QVector<quint64> vValue;
	QVector<QString> vUnit;
	QVector<quint64> hValue;
	QVector<QString> hUnit;
	for(quint64 i = 0; i < Signal::vDialValueCount; i++)
	    vValue.append(Signal::vDialValue[i]);
	for(quint64 i = 0; i < Signal::vDialUnitCount; i++)
	    vUnit.append(Signal::vDialUnit[i]);

        for(quint64 i = 0; i < Signal::hDialValueCount; i++)
            hValue.append(Signal::hDialValue[i]);
        for(quint64 i = 0; i < Signal::hDialUnitCount; i++)
            hUnit.append(Signal::hDialUnit[i]);

        _vDial = new dslDial(vDialValueCount, vDialValueStep, vValue, vUnit);
        _hDial = new dslDial(hDialValueCount, hDialValueStep, hValue, hUnit);
        _vDial->set_sel(0);
        _hDial->set_sel(0);

        _trig_vpos = 0;
        _trig_en = true;
    }
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
    _trig(0),
    _vDialActive(false),
    _hDialActive(false),
    _acCoupling(false),
    _active(true)
{
    if (_type == DS_DSO) {
        QVector<quint64> vValue;
        QVector<QString> vUnit;
        QVector<quint64> hValue;
        QVector<QString> hUnit;
        for(quint64 i = 0; i < Signal::vDialValueCount; i++)
            vValue.append(Signal::vDialValue[i]);
        for(quint64 i = 0; i < Signal::vDialUnitCount; i++)
            vUnit.append(Signal::vDialUnit[i]);

        for(quint64 i = 0; i < Signal::hDialValueCount; i++)
            hValue.append(Signal::hDialValue[i]);
        for(quint64 i = 0; i < Signal::hDialUnitCount; i++)
            hUnit.append(Signal::hDialUnit[i]);

        _vDial = new dslDial(Signal::vDialValueCount, Signal::vDialValueStep, vValue, vUnit);
        _hDial = new dslDial(Signal::hDialValueCount, Signal::hDialValueStep, hValue, hUnit);
        _vDial->set_sel(0);
        _hDial->set_sel(0);
    }
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
    return 2 * Margin + SquareNum * SquareWidth + 1.5 * SquareWidth;
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
    else if (trig == EDGETRIG)
        ds_trigger_probe_set(_index_list.front(), 'C', 'X');
}

bool Signal::get_vDialActive() const
{
    return _vDialActive;
}

void Signal::set_vDialActive(bool active)
{
    _vDialActive = active;
}

bool Signal::go_vDialPre()
{
    assert(_type == DS_DSO);

    if (!_vDial->isMin()) {
        _vDial->set_sel(_vDial->get_sel() - 1);
        return true;
    } else {
        return false;
    }
}

bool Signal::go_vDialNext()
{
    assert(_type == DS_DSO);

    if (!_vDial->isMax()) {
        _vDial->set_sel(_vDial->get_sel() + 1);
        return true;
    } else {
        return false;
    }
}

bool Signal::get_hDialActive() const
{
    return _hDialActive;
}

void Signal::set_hDialActive(bool active)
{
    _hDialActive = active;
}

bool Signal::go_hDialPre()
{
    assert(_type == DS_DSO);

    if (!_hDial->isMin()) {
        _hDial->set_sel(_hDial->get_sel() - 1);
        return true;
    } else {
        return false;
    }
}

bool Signal::go_hDialNext()
{
    assert(_type == DS_DSO);

    if (!_hDial->isMax()) {
        _hDial->set_sel(_hDial->get_sel() + 1);
        return true;
    } else {
        return false;
    }
}

quint64 Signal::get_vDialValue() const
{
    return _vDial->get_value();
}

quint64 Signal::get_hDialValue() const
{
    return _hDial->get_value();
}

uint16_t Signal::get_vDialSel() const
{
    return _vDial->get_sel();
}

uint16_t Signal::get_hDialSel() const
{
    return _hDial->get_sel();
}

bool Signal::get_acCoupling() const
{
    return _acCoupling;
}

void Signal::set_acCoupling(bool coupling)
{
    _acCoupling = coupling;
}

bool Signal::get_active() const
{
    return _active;
}

void Signal::set_active(bool active)
{
    _active = active;
}

int Signal::get_zeroPos() const
{
    return _zeroPos;
}

void Signal::set_zeroPos(int pos)
{
    _zeroPos = pos;
}

int Signal::get_windowHeight() const
{
    return _windowHeight;
}

void Signal::set_windowHeight(int height)
{
    _windowHeight = height;
}

int Signal::get_trig_vpos() const
{
    return _trig_vpos;
}

void Signal::set_trig_vpos(int value)
{
    _trig_vpos = value;
}

void Signal::paint_label(QPainter &p, int y, int right, bool hover, int action)
{
    compute_text_size(p);

    const QRectF color_rect = get_rect("color", y, right);
    const QRectF name_rect  = get_rect("name",  y, right);
    const QRectF label_rect = get_rect("label", (_type == DS_DSO) ? _zeroPos : y, right);

    p.setRenderHint(QPainter::Antialiasing);
    // Paint the ColorButton
    p.setPen(Qt::transparent);
    p.setBrush(_active ? _colour : dsDisable);
    p.drawRect(color_rect);

    // Paint the signal name
    p.setPen(_active ? Qt::black : dsDisable);
    p.drawText(name_rect, Qt::AlignLeft | Qt::AlignVCenter, _name);

    // Paint the trigButton
    if (_type == DS_LOGIC) {     
        const QRectF posTrig_rect  = get_rect("posTrig",  y, right);
        const QRectF higTrig_rect  = get_rect("higTrig",  y, right);
        const QRectF negTrig_rect  = get_rect("negTrig",  y, right);
        const QRectF lowTrig_rect  = get_rect("lowTrig",  y, right);
        const QRectF edgeTrig_rect = get_rect("edgeTrig", y, right);

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
        p.setBrush(((hover && action == EDGETRIG) || (_trig == EDGETRIG)) ?
                       dsYellow :
                       dsBlue);
        p.drawRect(edgeTrig_rect);

        p.setPen(QPen(Qt::blue, 1, Qt::DotLine));
        p.setBrush(Qt::transparent);
        p.drawLine(posTrig_rect.right(), posTrig_rect.top() + 3,
                   posTrig_rect.right(), posTrig_rect.bottom() - 3);
        p.drawLine(higTrig_rect.right(), higTrig_rect.top() + 3,
                   higTrig_rect.right(), higTrig_rect.bottom() - 3);
        p.drawLine(negTrig_rect.right(), negTrig_rect.top() + 3,
                   negTrig_rect.right(), negTrig_rect.bottom() - 3);
        p.drawLine(lowTrig_rect.right(), lowTrig_rect.top() + 3,
                   lowTrig_rect.right(), lowTrig_rect.bottom() - 3);

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

        p.drawLine(edgeTrig_rect.left() + 5, edgeTrig_rect.top() + 5,
                   edgeTrig_rect.center().x() - 2, edgeTrig_rect.top() + 5);
        p.drawLine(edgeTrig_rect.center().x() + 2 , edgeTrig_rect.top() + 5,
                   edgeTrig_rect.right() - 5, edgeTrig_rect.top() + 5);
        p.drawLine(edgeTrig_rect.center().x(), edgeTrig_rect.top() + 7,
                   edgeTrig_rect.center().x(), edgeTrig_rect.bottom() - 7);
        p.drawLine(edgeTrig_rect.left() + 5, edgeTrig_rect.bottom() - 5,
                   edgeTrig_rect.center().x() - 2, edgeTrig_rect.bottom() - 5);
        p.drawLine(edgeTrig_rect.center().x() + 2, edgeTrig_rect.bottom() - 5,
                   edgeTrig_rect.right() - 5, edgeTrig_rect.bottom() - 5);
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
    } else if (_type == DS_DSO) {
        const QRectF vDial_rect = get_rect("vDial", y, right);
        const QRectF hDial_rect = get_rect("hDial", y, right);
        const QRectF acdc_rect = get_rect("acdc", y, right);
        const QRectF chEn_rect = get_rect("chEn", y, right);

        QColor vDial_color = _vDialActive ? dsActive : dsDisable;
        QColor hDial_color = _hDialActive ? dsActive : dsDisable;
        _vDial->paint(p, vDial_rect, vDial_color);
        _hDial->paint(p, hDial_rect, hDial_color);

        p.setPen(Qt::transparent);
        p.setBrush((hover && action == CHEN) ? _colour.darker() : _colour);
        p.drawRect(chEn_rect);
        p.setPen(Qt::white);
        p.drawText(chEn_rect, Qt::AlignCenter | Qt::AlignVCenter, _active ? "EN" : "DIS");

        p.setPen(Qt::transparent);
        p.setBrush(_active ? ((hover && action == ACDC) ? _colour.darker() : _colour) : dsDisable);
        p.drawRect(acdc_rect);
        p.setPen(Qt::white);
        p.drawText(acdc_rect, Qt::AlignCenter | Qt::AlignVCenter, _acCoupling ? "AC" : "DC");
    }

	// Paint the label
    if (_active) {
        const QPointF points[] = {
            label_rect.topLeft(),
            label_rect.topRight(),
            QPointF(right, (_type == DS_DSO) ? _zeroPos : y),
            label_rect.bottomRight(),
            label_rect.bottomLeft()
        };

        p.setPen(Qt::transparent);
        if (_type == DS_DSO)
            p.setBrush(((hover && action == LABEL) || _selected) ? _colour.darker() : _colour);
        else
            p.setBrush(((hover && action == LABEL) || _selected) ? dsYellow : dsBlue);
        p.drawPolygon(points, countof(points));

        p.setPen(QPen(Qt::blue, 1, Qt::DotLine));
        p.setBrush(Qt::transparent);
        p.drawLine(label_rect.right(), label_rect.top() + 3,
                    label_rect.right(), label_rect.bottom() - 3);

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
}

void Signal::paint_trig(QPainter &p, int left, int right, bool hover)
{
    if (_type == DS_DSO) {
        const QRectF label_rect = get_rect("dsoTrig", -1, right);
        // Paint the trig line
        if (_trig_en) {
            const QPointF points[] = {
                QPointF(right - label_rect.width()*1.5, _trig_vpos),
                label_rect.topLeft(),
                label_rect.topRight(),
                label_rect.bottomRight(),
                label_rect.bottomLeft()
            };

            p.setPen(Qt::transparent);
            p.setBrush(_colour);
            p.drawPolygon(points, countof(points));

            // paint the _trig_vpos line
            p.setPen(QPen(_colour, hover ? 2 : 1, Qt::DashLine));
            p.drawLine(left, _trig_vpos, right - label_rect.width()*1.5, _trig_vpos);

            // Paint the text
            p.setPen(Qt::white);
            p.drawText(label_rect, Qt::AlignCenter | Qt::AlignVCenter, "T");
        }
    }
}

int Signal::pt_in_rect(int y, int right, const QPoint &point)
{
    const QRectF color = get_rect("color", y, right);
    const QRectF name  = get_rect("name", y, right);
    const QRectF posTrig = get_rect("posTrig", y, right);
    const QRectF higTrig = get_rect("higTrig", y, right);
    const QRectF negTrig = get_rect("negTrig", y, right);
    const QRectF lowTrig = get_rect("lowTrig", y, right);
    const QRectF edgeTrig = get_rect("edgeTrig", y, right);
    const QRectF label = get_rect("label", (_type == DS_DSO) ? _zeroPos : y, right);
    const QRectF vDial = get_rect("vDial", y, right);
    const QRectF hDial = get_rect("hDial", y, right);
    const QRectF chEn = get_rect("chEn", y, right);
    const QRectF acdc = get_rect("acdc", y, right);
    const QRectF dsoTrig = get_rect("dsoTrig", 0, right);

    if (color.contains(point) && _active)
        return COLOR;
    else if (name.contains(point) && _active)
        return NAME;
    else if (posTrig.contains(point) && _type == DS_LOGIC)
        return POSTRIG;
    else if (higTrig.contains(point) && _type == DS_LOGIC)
        return HIGTRIG;
    else if (negTrig.contains(point) && _type == DS_LOGIC)
        return NEGTRIG;
    else if (lowTrig.contains(point) && _type == DS_LOGIC)
        return LOWTRIG;
    else if (edgeTrig.contains(point) && _type == DS_LOGIC)
        return EDGETRIG;
    else if (label.contains(point) && _active)
        return LABEL;
    else if (vDial.contains(point) && _type == DS_DSO && _active)
        return VDIAL;
    else if (hDial.contains(point) && _type == DS_DSO && _active)
        return HDIAL;
    else if (chEn.contains(point) && _type == DS_DSO)
        return CHEN;
    else if (acdc.contains(point) && _type == DS_DSO && _active)
        return ACDC;
    else if (dsoTrig.contains(point) && _type == DS_DSO && _active)
        return DSOTRIG;
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
    else if (!strcmp(s, "dsoTrig"))
        return QRectF(
            right - label_size.width(),
            _trig_vpos - SquareWidth / 2,
            label_size.width(), label_size.height());
    else
        return QRectF(
            2,
            y - SquareWidth / 2,
            SquareWidth, SquareWidth);
}

} // namespace view
} // namespace pv
