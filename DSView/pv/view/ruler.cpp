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
#include "dsosignal.h"
#include "../dsvdef.h"

#include <assert.h>
#include <math.h>
#include <limits.h>
#include <cmath>

#include <QMouseEvent>
#include <QPainter>
#include <QStyleOption>
#include "../appcontrol.h"
#include "../config/appconfig.h"
#include "../ui/fn.h"


using namespace std;
using namespace Qt;

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

const int Ruler::HoverArrowSize = 4;

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

Ruler::Ruler(View &parent) :
	QWidget(&parent),
	_view(parent),
    _cursor_sel_visible(false),
    _cursor_go_visible(false),
    _cursor_sel_x(-1),
    _grabbed_marker(NULL),
    _hitCursor(false),
    _curs_moved(false)
{
	setMouseTracking(true);

	connect(&_view, SIGNAL(hover_point_changed()),
		this, SLOT(hover_point_changed()));
}

QString Ruler::format_freq(double period, unsigned precision)
{   
    if (period <= 0) {
        return View::Unknown_Str;
    } else {
        const int order = ceil(log10f(period));
        assert(order >= FirstSIPrefixPower);
        const int prefix = ceil((order - FirstSIPrefixPower) / 3.0f);
        const double multiplier = pow(10.0, max(-prefix * 3.0 - FirstSIPrefixPower, 0.0));

        /*
        QString s;
        QTextStream ts(&s);
        ts.setRealNumberPrecision(precision);
        ts << fixed << 1 / (period  * multiplier) <<
            FreqPrefixes[prefix] << "Hz";
        return s;
        */
       
        char buffer[20] = {0};
        char format[10] = {0};
        QString units = FreqPrefixes[prefix] + "Hz";
        sprintf(format, "%%.%df", (int)precision);       
        sprintf(buffer, format, 1 / (period * multiplier));
        strcat(buffer, units.toUtf8().data());
        return QString(buffer);
    }
}

QString Ruler::format_time(double t, int prefix,
    unsigned int precision)
{ 
    const double multiplier = pow(10.0, -prefix * 3 - FirstSIPrefixPower + 6.0);

    /*
	QString s;
	QTextStream ts(&s);
	ts.setRealNumberPrecision(precision);
    ts << fixed << forcesign << (t  * multiplier) / 1000000.0 <<
		SIPrefixes[prefix] << "s";
	return s;
    */

    char buffer[20] = {0};
    char format[10] = {0}; 
    QString units = SIPrefixes[prefix] + "s";
    double v = (t * multiplier) / 1000000.0;
    buffer[0] = v >= 0 ? '+' : '-';
    sprintf(format, "%%.%df", (int)precision);   
    sprintf(buffer + 1, format, v);
    strcat(buffer + 1, units.toUtf8().data());
    return QString(buffer);
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

    QFont font = p.font();
    float fSize = AppConfig::Instance().appOptions.fontSize;
    if (fSize > 10)
        fSize = 10;
    font.setPointSizeF(fSize);
    p.setFont(font);

    SigSession *session = AppControl::Instance()->GetSession();

    // Draw tick mark
    if (session->get_device()->get_work_mode() == DSO)
        draw_osc_tick_mark(p);
    else
        draw_logic_tick_mark(p);

    p.setRenderHint(QPainter::Antialiasing, true);
	// Draw the hover mark
	draw_hover_mark(p);

    // Draw cursor selection
    if (_cursor_sel_visible || _cursor_go_visible) {
        draw_cursor_sel(p);
    }

	p.end();
}

void Ruler::mouseMoveEvent(QMouseEvent *e)
{
    (void)e;

    if (_grabbed_marker) {
        int msx = _view.hover_point().x();
        if (msx < 0)
            msx = 0;   
        int body_width = _view.get_body_width();
        if (msx > body_width)
            msx = body_width;

        uint64_t index = _view.pixel2index(msx);
        _grabbed_marker->set_index(index);
        _view.cursor_moving();
        _curs_moved = true;
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

void Ruler::mousePressEvent(QMouseEvent *event)
{
    if (event->button() & Qt::LeftButton) {
        bool visible;
        auto &cursor_list = _view.get_cursorList();

        if (!_cursor_sel_visible && cursor_list.size()) {
            _view.show_cursors(true);
            auto i = cursor_list.begin();

            while (i != cursor_list.end()) {
                const QRect cursor_rect((*i)->get_label_rect(rect(), visible));
                if ((*i)->get_close_rect(cursor_rect).contains(event->pos())) {
                    _view.del_cursor(*i);

                    if (cursor_list.empty()) {
                        _cursor_sel_visible = false;
                        _view.show_cursors(false);
                    }
                    _hitCursor = true;
                    break;
                }

                if (cursor_rect.contains(event->pos())) {
                    set_grabbed_cursor(*i);
                    _cursor_sel_visible = false;
                    _cursor_go_visible = false;
                    _hitCursor = true;
                    break;
                }
                i++;
            }
        }
    }
}

void Ruler::mouseReleaseEvent(QMouseEvent *event)
{
    bool updatedCursor = false;

    if (event->button() & Qt::LeftButton) {
        if (!_hitCursor && !_grabbed_marker) {
            if (!_cursor_go_visible) {
                if (!_cursor_sel_visible) {
                    _cursor_sel_x = event->pos().x();
                    _cursor_sel_visible = true;                                    
                } 
                else {
                    int overCursor;
                    int msx = _cursor_sel_x;
                    if (msx < 0)
                        msx = 0;
                    
                    int body_width = _view.get_body_width();
                    if (msx > body_width)
                        msx = body_width;

                    auto &cursor_list = _view.get_cursorList();
                    uint64_t index = _view.pixel2index(_cursor_sel_x);
                    overCursor = in_cursor_sel_rect(event->pos());

                    if (overCursor == 0) {
                        _view.add_cursor(CursorColor[cursor_list.size() % 8], index);
                        _view.show_cursors(true);
                        updatedCursor = true;
                    }
                    else if (overCursor > 0) {
                        auto i = cursor_list.begin();

                        while (--overCursor != 0){
                            i++;
                        }

                        (*i)->set_index(index);
                        updatedCursor = true;
                        _view.cursor_moved();
                    }
                    _cursor_sel_visible = false;
                }
            } 
            else {
                int overCursor;
                overCursor = in_cursor_sel_rect(event->pos());
                if (overCursor > 0) {
                    _view.set_cursor_middle(overCursor - 1);
                }

                _cursor_go_visible = false;
            }
        }

        if (_curs_moved && _grabbed_marker) {
            rel_grabbed_cursor();
            _hitCursor = false;
            _curs_moved = false;
            _view.cursor_moved();
        }

        if (_hitCursor && !_grabbed_marker) {
            _hitCursor = false;
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
            auto &cursor_list = _view.get_cursorList();

            if (overCursor > 0) {
                auto i = cursor_list.begin();

                while (--overCursor != 0){
                        i++;
                }

                _view.del_cursor(*i);
            }

            if (cursor_list.empty()) {
                _cursor_sel_visible = false;
                _view.show_cursors(false);
            }
        }
    }

    update();
    if (updatedCursor) {
        //const QRect reDrawRect = QRect(_cursor_sel_x - 1, 0, 3, _view.viewport()->height());
        //_view.viewport()->update(reDrawRect);
        _view.viewport()->update();
    }
}

void Ruler::draw_logic_tick_mark(QPainter &p)
{
    using namespace Qt;

    if (_view.session().get_device()->have_instance() == false){
        return;
    }

    const double SpacingIncrement = 32.0;
    const double MinValueSpacing = 16.0;
    const int ValueMargin = 5;
    const double abs_min_period = 10.0 / _view.session().cur_snap_samplerate();

    double min_width = SpacingIncrement;
    double typical_width;
    double tick_period = 0;
    double scale = _view.scale();
    int64_t offset = _view.offset();

    const uint64_t cur_period_scale = ceil((scale * min_width) / abs_min_period);

    // Find tick spacing, and number formatting that does not cause
    // value to collide.
    _min_period = cur_period_scale * abs_min_period;

    const int order = (int)floorf(log10f(scale * _view.get_view_width()));
    //const double order_decimal = pow(10, order);
    const unsigned int prefix = (order - FirstSIPrefixPower) / 3;
    _cur_prefix = prefix;
    assert(prefix < countof(SIPrefixes));
    typical_width = p.boundingRect(0, 0, INT_MAX, INT_MAX,
        AlignLeft | AlignTop, format_time(offset * scale,
        prefix)).width() + MinValueSpacing;
    do
    {
        tick_period += _min_period;

    } while(typical_width > tick_period / scale);

    const int text_height = p.boundingRect(0, 0, INT_MAX, INT_MAX,
        AlignLeft | AlignTop, "8").height();

    // Draw the tick marks
    QColor fore(QWidget::palette().color(QWidget::foregroundRole()));
    fore.setAlpha(View::ForeAlpha);
    p.setPen(fore);

    const double minor_tick_period = tick_period / MinPeriodScale;
    const int minor_order = (int)floorf(log10f(minor_tick_period));
    //const double minor_order_decimal = pow(10, minor_order);
    const unsigned int minor_prefix = (minor_order - FirstSIPrefixPower) / 3;
    assert(minor_prefix < countof(SIPrefixes));

    const double first_major_division =
        floor(offset * scale / tick_period);
    const double first_minor_division =
        floor(offset * scale / minor_tick_period + 1);
    const double t0 = first_major_division * tick_period;

    int division = (int)round(first_minor_division -
        first_major_division * MinPeriodScale) - 1;

    const int major_tick_y1 = text_height + ValueMargin * 3;
    const int tick_y2 = height();
    const int minor_tick_y1 = (major_tick_y1 + tick_y2) / 2;

    int x;

    const double inc_text_width = p.boundingRect(0, 0, INT_MAX, INT_MAX,
                                                 AlignLeft | AlignTop,
                                                 format_time(minor_tick_period,
                                                             minor_prefix)).width() + MinValueSpacing;
    do {
        const double t = t0 + division * minor_tick_period;
        const double major_t = t0 + floor(division / MinPeriodScale) * tick_period;

        x = t / scale - offset;

        if (division % MinPeriodScale == 0)
        {
            // Draw a major tick
            p.drawText(x, 2 * ValueMargin, 0, text_height,
                AlignCenter | AlignTop | TextDontClip,
                format_time(t, prefix));
            p.drawLine(QPoint(x, major_tick_y1),
                QPoint(x, tick_y2));
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
            p.drawLine(QPoint(x, minor_tick_y1),
                QPoint(x, tick_y2));
        }

        division++;

    } while (x < rect().right());

    // Draw the cursors
    auto &cursor_list = _view.get_cursorList();

    if (cursor_list.size()) {
        auto i = cursor_list.begin();
        int index = 1;

        while (i != cursor_list.end()) {
            (*i)->paint_label(p, rect(), prefix, index, _view.session().is_stopped_status());
            index++;
            i++;
        }
    }

    if (_view.trig_cursor_shown()) {
        _view.get_trig_cursor()->paint_fix_label(p, rect(), prefix, 'T', _view.get_trig_cursor()->colour(), false);
    }
    if (_view.search_cursor_shown()) {
        _view.get_search_cursor()->paint_fix_label(p, rect(), prefix, 'S', _view.get_search_cursor()->colour(), true);
    }
}

void Ruler::draw_osc_tick_mark(QPainter &p)
{
    using namespace Qt;

    const double MinValueSpacing = 16.0;
    const int ValueMargin = 5;

    double typical_width;
    double tick_period = 0;
    double scale = _view.scale();
    int64_t offset = 0;

    // Find tick spacing, and number formatting that does not cause
    // value to collide.
    _min_period = _view.session().get_device()->get_time_base() * std::pow(10.0, -9.0);

    const int order = (int)floorf(log10f(scale * _view.get_view_width()));
    //const double order_decimal = pow(10, order);
    const unsigned int prefix = (order - FirstSIPrefixPower) / 3;
    _cur_prefix = prefix;
    assert(prefix < countof(SIPrefixes));
    typical_width = p.boundingRect(0, 0, INT_MAX, INT_MAX,
        AlignLeft | AlignTop, format_time(offset * scale,
        prefix)).width() + MinValueSpacing;
    do
    {
        tick_period += _min_period;

    } while(typical_width > tick_period / scale);

    const int text_height = p.boundingRect(0, 0, INT_MAX, INT_MAX,
        AlignLeft | AlignTop, "8").height();

    // Draw the tick marks
    QColor fore(QWidget::palette().color(QWidget::foregroundRole()));
    fore.setAlpha(View::ForeAlpha);
    p.setPen(fore);

    const double minor_tick_period = tick_period / MinPeriodScale;
    const int minor_order = (int)floorf(log10f(minor_tick_period));
    //const double minor_order_decimal = pow(10, minor_order);
    const unsigned int minor_prefix = (minor_order - FirstSIPrefixPower) / 3;
    assert(minor_prefix < countof(SIPrefixes));

    const double first_major_division =
        floor(offset * scale / tick_period);
    const double first_minor_division =
        floor(offset * scale / minor_tick_period + 1);
    const double t0 = first_major_division * tick_period;

    int division = (int)round(first_minor_division -
        first_major_division * MinPeriodScale) - 1;

    const int major_tick_y1 = text_height + ValueMargin * 3;
    const int tick_y2 = height();
    const int minor_tick_y1 = (major_tick_y1 + tick_y2) / 2;

    int x;

    const double inc_text_width = p.boundingRect(0, 0, INT_MAX, INT_MAX,
                                                 AlignLeft | AlignTop,
                                                 format_time(minor_tick_period,
                                                             minor_prefix)).width() + MinValueSpacing;
    do {
        const double t = t0 + division * minor_tick_period;
        const double major_t = t0 + floor(division / MinPeriodScale) * tick_period;

        x = t / scale - offset;

        if (division % MinPeriodScale == 0)
        {
            // Draw a major tick
            p.drawText(x, 2 * ValueMargin, 0, text_height,
                AlignCenter | AlignTop | TextDontClip,
                format_time(t, prefix));
            p.drawLine(QPoint(x, major_tick_y1), QPoint(x, tick_y2));
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
            p.drawLine(QPoint(x, minor_tick_y1), QPoint(x, tick_y2));
        }

        division++;

    } while (x < rect().right());

    // Draw the cursors
    auto &cursor_list = _view.get_cursorList();

    if (!cursor_list.empty()) {
        auto i = cursor_list.begin();
        int index = 1;

        while (i != cursor_list.end()) {
            (*i)->paint_label(p, rect(), prefix, index, _view.session().is_stopped_status());
            index++;
            i++;
        }
    }
    
    if (_view.trig_cursor_shown()) {
        _view.get_trig_cursor()->paint_fix_label(p, rect(), prefix, 'T', _view.get_trig_cursor()->colour(), false);
    }
    if (_view.search_cursor_shown()) {
        _view.get_search_cursor()->paint_fix_label(p, rect(), prefix, 'S', _view.get_search_cursor()->colour(), true);
    }
}

void Ruler::draw_hover_mark(QPainter &p)
{
    const double x = _view.hover_point().x();

	if (x == -1 || _grabbed_marker)
		return;

    QColor fore(QWidget::palette().color(QWidget::foregroundRole()));
    p.setPen(fore);
    p.setBrush(fore);

	const int b = height() - 1;
    for (int i = 0; i < HoverArrowSize; i++)
        for (int j = -i; j <= i; j++)
            p.drawPoint(x-j, b-i);
}

void Ruler::draw_cursor_sel(QPainter &p)
{
    if (_cursor_sel_x == -1)
        return;

    p.setPen(QPen(Qt::NoPen));
    p.setBrush(View::Blue);

    const QPoint pos = QPoint(_view.hover_point().x(), _view.hover_point().y());
    if (in_cursor_sel_rect(pos) == 0)
        p.setBrush(View::Orange);

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

    auto &cursor_list = _view.get_cursorList();

    if (!cursor_list.empty()) {
        int index = 1;
        auto i = cursor_list.begin();

        while (i != cursor_list.end()) {
            const QRectF cursorRect = get_cursor_sel_rect(index);
            p.setPen(QPen(Qt::black, 1, Qt::DotLine));
            p.drawLine(cursorRect.left(), cursorRect.top() + 3,
                       cursorRect.left(), cursorRect.bottom() - 3);
            p.setPen(QPen(Qt::NoPen));

            if (in_cursor_sel_rect(pos) == index)
                p.setBrush(View::Orange);
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

    auto &cursor_list = _view.get_cursorList();

    for (unsigned int i = 0; i < cursor_list.size() + 1; i++) {
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

double Ruler::get_min_period()
{
    return _min_period / MinPeriodScale;
}

} // namespace view
} // namespace pv
