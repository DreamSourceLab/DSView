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


#include "ruler.h"

#include "cursor.h"
#include "view.h"
#include "viewport.h"
#include "../sigsession.h"
#include "../device/devinst.h"
#include "dsosignal.h"

#include <extdef.h>

#include <assert.h>
#include <math.h>
#include <limits.h>

#include <QMouseEvent>
#include <QPainter>
#include <QTextStream>
#include <QStyleOption>

#include <boost/shared_ptr.hpp>

using namespace boost;
using namespace std;

namespace pv {
namespace view {

const int Ruler::MinorTickSubdivision = 4;
const int Ruler::ScaleUnits[3] = {1, 2, 5};
const int Ruler::MinPeriodScale = 10;

const QString Ruler::SIPrefixes[9] =
	{"f", "p", "n", QChar(0x03BC), "m", "", "k", "M", "G"};
const QString Ruler::FreqPrefixes[9] =
    {"P", "T", "G", "M", "K", "", "", "", ""};
const int Ruler::FirstSIPrefixPower = -15;
const int Ruler::pricision = 2;

const int Ruler::HoverArrowSize = 5;

const int Ruler::CursorSelWidth = 20;
const QColor Ruler::CursorColor[8] =
    {QColor(25, 189, 155, 200),
     QColor(46, 205, 113, 200),
     QColor(53, 152, 220, 200),
     QColor(154, 89, 181, 200),
     QColor(52, 73, 94, 200),
     QColor(242, 196, 15, 200),
     QColor(231, 126, 34, 200),
     QColor(232, 76, 61, 200)};

const QColor Ruler::dsBlue = QColor(17, 133, 209,  255);
const QColor Ruler::dsYellow = QColor(238, 178, 17, 255);
const QColor Ruler::dsRed = QColor(213, 15, 37, 255);
const QColor Ruler::dsGreen = QColor(0, 153, 37, 255);
const QColor Ruler::RULER_COLOR = QColor(255, 255, 255, 255);

const QColor Ruler::HitColor = dsYellow;
const QColor Ruler::WarnColor = dsRed;

Ruler::Ruler(View &parent) :
	QWidget(&parent),
	_view(parent),
    _cursor_sel_visible(false),
    _cursor_go_visible(false),
    _cursor_sel_x(-1),
	_grabbed_marker(NULL)
{
	setMouseTracking(true);

	connect(&_view, SIGNAL(hover_point_changed()),
		this, SLOT(hover_point_changed()));
}

QString Ruler::format_freq(double period, unsigned precision)
{
    if (period <= 0) {
        return "#####";
    } else {
        const int order = ceil(log10f(period));
        assert(order >= FirstSIPrefixPower);
        const int prefix = ceil((order - FirstSIPrefixPower) / 3.0f);
        const double multiplier = pow(10.0, max(-prefix * 3.0 - FirstSIPrefixPower, 0.0));

        QString s;
        QTextStream ts(&s);
        ts.setRealNumberPrecision(precision);
        ts << fixed << 1 / (period  * multiplier) <<
            FreqPrefixes[prefix] << "Hz";
        return s;
    }
}

QString Ruler::format_time(double t, int prefix,
    unsigned int precision)
{
    const double multiplier = pow(10.0, -prefix * 3 - FirstSIPrefixPower + 6.0);

	QString s;
	QTextStream ts(&s);
	ts.setRealNumberPrecision(precision);
    ts << fixed << forcesign << (t  * multiplier) / 1000000.0 <<
		SIPrefixes[prefix] << "s";
	return s;
}

QString Ruler::format_time(double t)
{
    return format_time(t, _cur_prefix);
}

QString Ruler::format_real_time(uint64_t delta_index, uint64_t sample_rate)
{
    uint64_t delta_time = std::pow(10, 12) / sample_rate * delta_index;

    if (delta_time == 0)
        return "0";

    int zero = 0;
    int prefix = (int)floor(log10(delta_time));
    while(delta_time == (delta_time/10*10)) {
        delta_time /= 10;
        zero++;
    }

    return format_time(delta_time / std::pow(10.0, 12-zero), prefix/3+1, prefix/3*3 > zero ? prefix/3*3 - zero : 0);
}

QString Ruler::format_real_freq(uint64_t delta_index, uint64_t sample_rate)
{
    const double delta_period = delta_index * 1.0 / sample_rate;
    return format_freq(delta_period);
}

TimeMarker* Ruler::get_grabbed_cursor()
{
    return _grabbed_marker;
}

void Ruler::set_grabbed_cursor(TimeMarker *grabbed_marker)
{
    _grabbed_marker = grabbed_marker;
    _grabbed_marker->set_grabbed(true);
}

void Ruler::rel_grabbed_cursor()
{
    if (_grabbed_marker) {
        _grabbed_marker->set_grabbed(false);
        _grabbed_marker = NULL;
    }
}

void Ruler::paintEvent(QPaintEvent*)
{
    QStyleOption o;
    o.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &o, &p, this);

    //p.begin(this);
    //QPainter p(this);
    //p.setRenderHint(QPainter::Antialiasing);

    // Draw tick mark
    draw_logic_tick_mark(p);

    p.setRenderHint(QPainter::Antialiasing);
	// Draw the hover mark
	draw_hover_mark(p);

    // Draw cursor selection
    if (_cursor_sel_visible || _cursor_go_visible)
        draw_cursor_sel(p);

	p.end();
}

void Ruler::mouseMoveEvent(QMouseEvent *e)
{
    (void)e;

    if (_grabbed_marker) {
        _grabbed_marker->set_index((_view.offset() +
            _view.hover_point().x() * _view.scale()) * _view.session().cur_samplerate());
    }

    update();
    _view.viewport()->update();
}

void Ruler::leaveEvent(QEvent *)
{
    _cursor_sel_visible = false;
    _cursor_go_visible = false;
    update();
}

void Ruler::mousePressEvent(QMouseEvent *e)
{
    (void)e;
}

void Ruler::mouseReleaseEvent(QMouseEvent *event)
{
    bool addCursor = false;
    if (event->button() & Qt::LeftButton) {
        bool hitCursor = false;
        if (!_cursor_sel_visible & !_view.get_cursorList().empty()) {
            _view.show_cursors(true);
            if (_grabbed_marker) {
                rel_grabbed_cursor();
                hitCursor = true;
            } else {
                list<Cursor*>::iterator i = _view.get_cursorList().begin();
                while (i != _view.get_cursorList().end()) {
                    if ((*i)->get_close_rect(rect()).contains(event->pos())) {
                        _view.del_cursor(*i);
                        if (_view.get_cursorList().empty()) {
                            _cursor_sel_visible = false;
                            _view.show_cursors(false);
                        }
                        hitCursor = true;
                        break;
                    }
                    if ((*i)->get_label_rect(rect()).contains(event->pos())) {
                        set_grabbed_cursor(*i);
                        _cursor_sel_visible = false;
                        _cursor_go_visible = false;
                        hitCursor = true;
                        break;
                    }
                    i++;
                }
            }
        }
        if (!hitCursor && !_grabbed_marker) {
            if (!_cursor_go_visible) {
                if (!_cursor_sel_visible) {
                    _cursor_sel_x = event->pos().x();
                    _cursor_sel_visible = true;
                } else {
                    int overCursor;
                    uint64_t index = (_view.offset() + (_cursor_sel_x + 0.5) * _view.scale()) * _view.session().cur_samplerate();
                    overCursor = in_cursor_sel_rect(event->pos());
                    if (overCursor == 0) {
                        _view.add_cursor(CursorColor[_view.get_cursorList().size() % 8], index);
                        _view.show_cursors(true);
                        addCursor = true;
                    } else if (overCursor > 0) {
                        list<Cursor*>::iterator i = _view.get_cursorList().begin();
                        while (--overCursor != 0)
                                i++;
                        (*i)->set_index(index);
                    }
                    _cursor_sel_visible = false;
                }
            } else {
                int overCursor;
                overCursor = in_cursor_sel_rect(event->pos());
                if (overCursor > 0) {
                    _view.set_cursor_middle(overCursor - 1);
                }

                _cursor_go_visible = false;
            }
        }
    }

    if (event->button() & Qt::RightButton) {
        if (!_cursor_sel_visible) {
            if (!_cursor_go_visible) {
                _cursor_sel_x = event->pos().x();
                _cursor_go_visible = true;
            }
        } else {
            int overCursor;
            overCursor = in_cursor_sel_rect(event->pos());
            if (overCursor > 0) {
                list<Cursor*>::iterator i = _view.get_cursorList().begin();
                while (--overCursor != 0)
                        i++;
                _view.del_cursor(*i);
            }
            if (_view.get_cursorList().empty()) {
                _cursor_sel_visible = false;
                _view.show_cursors(false);
            }
        }
    }

    update();
    if (addCursor) {
        const QRect reDrawRect = QRect(_cursor_sel_x - 1, 0, 3, _view.viewport()->height());
        _view.viewport()->update(reDrawRect);
    }
}

void Ruler::draw_tick_mark(QPainter &p)
{
    using namespace Qt;

    const double SpacingIncrement = 32.0;
    const double MinValueSpacing = 16.0;
    const int ValueMargin = 15;

    double min_width = SpacingIncrement, typical_width;
    double tick_period;
    unsigned int prefix;

    // Find tick spacing, and number formatting that does not cause
    // value to collide.
    do
    {
        _min_period = _view.scale() * min_width;

        //const int order = (int)floorf(log10f(_min_period));
        const int order = ceil(log10f(_min_period));
        const double order_decimal = pow(10.0, static_cast<double>(order));

        unsigned int unit = 0;

        do
        {
            tick_period = order_decimal * ScaleUnits[unit++];
        } while (tick_period < _min_period && unit < countof(ScaleUnits));

        prefix = ceil((order - FirstSIPrefixPower) / 3.0f);
        assert(prefix < countof(SIPrefixes));


        typical_width = p.boundingRect(0, 0, INT_MAX, INT_MAX,
            AlignLeft | AlignTop, format_time(_view.offset(),
            prefix)).width() + MinValueSpacing;

        min_width += SpacingIncrement;

    } while(typical_width > tick_period / _view.scale());

    const int text_height = p.boundingRect(0, 0, INT_MAX, INT_MAX,
        AlignLeft | AlignTop, "8").height();

    // Draw the tick marks
    p.setPen(dsBlue);

    const double minor_tick_period = tick_period / MinorTickSubdivision;
    const double first_major_division =
        floor(_view.offset() / tick_period);
    const double first_minor_division =
        ceil(_view.offset() / minor_tick_period);
    const double t0 = first_major_division * tick_period;

    int division = (int)round(first_minor_division -
        first_major_division * MinorTickSubdivision) - 1;

    const int major_tick_y1 = text_height + ValueMargin * 3;
    const int tick_y2 = height();
    const int minor_tick_y1 = (major_tick_y1 + tick_y2) / 2;

    double x;

    do {
        const double t = t0 + division * minor_tick_period;
        x = (t - _view.offset()) / _view.scale();

        if (division % MinorTickSubdivision == 0)
        {
            // Draw a major tick
            p.drawText(x, 2 * ValueMargin, 0, text_height,
                AlignCenter | AlignTop | TextDontClip,
                format_time(t, prefix));
            p.drawLine(QPointF(x, major_tick_y1),
                QPointF(x, tick_y2));
        }
        else
        {
            // Draw a minor tick
            p.drawLine(QPointF(x, minor_tick_y1),
                QPointF(x, tick_y2));
        }

        division++;

    } while (x < _view.get_view_width());

    // Draw the cursors
    if (!_view.get_cursorList().empty()) {
        list<Cursor*>::iterator i = _view.get_cursorList().begin();
        int index = 1;
        while (i != _view.get_cursorList().end()) {
            (*i)->paint_label(p, rect(), prefix, index);
            index++;
            i++;
        }
    }
}

void Ruler::draw_logic_tick_mark(QPainter &p)
{
    using namespace Qt;

    const double SpacingIncrement = 32.0;
    const double MinValueSpacing = 16.0;
    const int ValueMargin = 5;
    const double abs_min_period = 10.0 / _view.session().cur_samplerate();

    double min_width = SpacingIncrement;
    double typical_width;
    double tick_period = 0;
    double scale = _view.scale();
    double offset = _view.offset();

    const uint64_t cur_period_scale = ceil((scale * min_width) / abs_min_period);

    // Find tick spacing, and number formatting that does not cause
    // value to collide.
    if (_view.session().get_device()->dev_inst()->mode == DSO) {
        _min_period = _view.session().get_device()->get_time_base() * std::pow(10.0, -9.0);
    } else {
        _min_period = cur_period_scale * abs_min_period;
    }
    const int order = (int)floorf(log10f(scale * _view.get_view_width()));
    //const double order_decimal = pow(10, order);
    const unsigned int prefix = (order - FirstSIPrefixPower) / 3;
    _cur_prefix = prefix;
    assert(prefix < countof(SIPrefixes));
    typical_width = p.boundingRect(0, 0, INT_MAX, INT_MAX,
        AlignLeft | AlignTop, format_time(offset,
        prefix)).width() + MinValueSpacing;
    do
    {
        tick_period += _min_period;

    } while(typical_width > tick_period / scale);

    const int text_height = p.boundingRect(0, 0, INT_MAX, INT_MAX,
        AlignLeft | AlignTop, "8").height();

    // Draw the tick marks
    p.setPen(Trace::DARK_FORE);

    const double minor_tick_period = tick_period / MinPeriodScale;
    const int minor_order = (int)floorf(log10f(minor_tick_period));
    //const double minor_order_decimal = pow(10, minor_order);
    const unsigned int minor_prefix = (minor_order - FirstSIPrefixPower) / 3;
    assert(minor_prefix < countof(SIPrefixes));

    const double first_major_division =
        floor(offset / tick_period);
    const double first_minor_division =
        floor(offset / minor_tick_period + 1);
    const double t0 = first_major_division * tick_period;

    int division = (int)round(first_minor_division -
        first_major_division * MinPeriodScale) - 1;

    const int major_tick_y1 = text_height + ValueMargin * 3;
    const int tick_y2 = height();
    const int minor_tick_y1 = (major_tick_y1 + tick_y2) / 2;

    double x;

    const double inc_text_width = p.boundingRect(0, 0, INT_MAX, INT_MAX,
                                                 AlignLeft | AlignTop,
                                                 format_time(minor_tick_period,
                                                             minor_prefix)).width() + MinValueSpacing;
    do {
        const double t = t0 + division * minor_tick_period;
        const double major_t = t0 + floor(division / MinPeriodScale) * tick_period;

        x = (t - offset) / scale;

        if (division % MinPeriodScale == 0)
        {
            // Draw a major tick
            p.drawText(x, 2 * ValueMargin, 0, text_height,
                AlignCenter | AlignTop | TextDontClip,
                format_time(t, prefix));
            p.drawLine(QPointF(x, major_tick_y1),
                QPointF(x, tick_y2));
        }
        else
        {
            // Draw a minor tick
            if (minor_tick_period / scale > 2 * typical_width)
                p.drawText(x, 2 * ValueMargin, 0, text_height,
                    AlignCenter | AlignTop | TextDontClip,
                    format_time(t, prefix));
            //else if ((tick_period / scale > width() / 4) && (minor_tick_period / scale > inc_text_width))
            else if (minor_tick_period / scale > 1.1 * inc_text_width ||
                     tick_period / scale > _view.get_view_width())
                p.drawText(x, 2 * ValueMargin, 0, minor_tick_y1 + ValueMargin,
                    AlignCenter | AlignTop | TextDontClip,
                    format_time(t - major_t, minor_prefix));
            p.drawLine(QPointF(x, minor_tick_y1),
                QPointF(x, tick_y2));
        }

        division++;

    } while (x < _view.get_view_width());

    // Draw the cursors
    if (!_view.get_cursorList().empty()) {
        list<Cursor*>::iterator i = _view.get_cursorList().begin();
        int index = 1;
        while (i != _view.get_cursorList().end()) {
            (*i)->paint_label(p, rect(), prefix, index);
            index++;
            i++;
        }
        _view.on_cursor_moved();
    }
    if (_view.trig_cursor_shown()) {
        _view.get_trig_cursor()->paint_fix_label(p, rect(), prefix, 'T', Trace::dsLightRed);
    }
    if (_view.search_cursor_shown()) {
        _view.get_search_cursor()->paint_fix_label(p, rect(), prefix, 'S', Trace::dsLightBlue);
    }
}

void Ruler::draw_hover_mark(QPainter &p)
{
    const double x = _view.hover_point().x();

	if (x == -1 || _grabbed_marker)
		return;

	p.setPen(QPen(Qt::NoPen));
    p.setBrush(RULER_COLOR);

	const int b = height() - 1;
	const QPointF points[] = {
		QPointF(x, b),
		QPointF(x - HoverArrowSize, b - HoverArrowSize),
		QPointF(x + HoverArrowSize, b - HoverArrowSize)
	};
	p.drawPolygon(points, countof(points));
}

void Ruler::draw_cursor_sel(QPainter &p)
{
    if (_cursor_sel_x == -1)
        return;

    p.setPen(QPen(Qt::NoPen));
    p.setBrush(dsBlue);

    const QPoint pos = QPoint(_view.hover_point().x(), _view.hover_point().y());
    if (in_cursor_sel_rect(pos) == 0)
        p.setBrush(HitColor);

    const int y = height();
    const QRectF selRect = get_cursor_sel_rect(0);
    const QPointF del_points[] = {
        QPointF(_cursor_sel_x + CursorSelWidth / 2, (selRect.top() + CursorSelWidth / 2)),
        QPointF((selRect.left() + selRect.right()) / 2, selRect.top()),
        selRect.topLeft(),
        selRect.bottomLeft(),
        QPointF((selRect.left() + selRect.right()) / 2, selRect.bottom())
    };
    const QPointF points[] = {
        QPointF(_cursor_sel_x, y),
        selRect.bottomLeft(),
        selRect.topLeft(),
        selRect.topRight(),
        selRect.bottomRight()
    };
    p.drawPolygon((_cursor_go_visible ? del_points : points), countof(points));

    if (!_view.get_cursorList().empty()) {
        int index = 1;
        list<Cursor*>::iterator i = _view.get_cursorList().begin();
        while (i != _view.get_cursorList().end()) {
            const QRectF cursorRect = get_cursor_sel_rect(index);
            p.setPen(QPen(Qt::black, 1, Qt::DotLine));
            p.drawLine(cursorRect.left(), cursorRect.top() + 3,
                       cursorRect.left(), cursorRect.bottom() - 3);
            p.setPen(QPen(Qt::NoPen));
            if (in_cursor_sel_rect(pos) == index)
                p.setBrush(HitColor);
            else
                p.setBrush(CursorColor[(index - 1)%8]);
            p.drawRect(cursorRect);
            p.setPen(Qt::black);
            p.drawText(cursorRect, Qt::AlignCenter | Qt::AlignVCenter, QString::number(index));
            index++;
            i++;
        }
    }
}

int Ruler::in_cursor_sel_rect(QPointF pos)
{
    if (_cursor_sel_x == -1)
        return -1;

    for (unsigned int i = 0; i < _view.get_cursorList().size() + 1; i++) {
        const QRectF cursorRect = get_cursor_sel_rect(i);
        if (cursorRect.contains(pos))
            return i;
    }

    return -1;
}

QRectF Ruler::get_cursor_sel_rect(int index)
{
    if (_cursor_sel_x == -1)
        return QRectF(-1, -1, 0, 0);
    const int y = height();
    return QRectF(_cursor_sel_x - (0.5 - index) * CursorSelWidth,
                  y - 1.3 * CursorSelWidth,
                  CursorSelWidth, CursorSelWidth);
}

void Ruler::hover_point_changed()
{
	update();
}

const double Ruler::get_min_period() const
{
    return _min_period / MinPeriodScale;
}

} // namespace view
} // namespace pv
