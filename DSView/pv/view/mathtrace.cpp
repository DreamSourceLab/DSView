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
#include "mathtrace.h"
#include "../data/dso.h"
#include "../data/dsosnapshot.h"
#include "../data/mathstack.h"
#include "view.h"
#include "../sigsession.h"
#include "../device/devinst.h"
#include "../view/dsosignal.h"

#include <boost/foreach.hpp>

#include <QDebug>
#include <QTimer>

using namespace boost;
using namespace std;

namespace pv {
namespace view {

MathTrace::MathTrace(bool enable,
                     boost::shared_ptr<data::MathStack> math_stack,
                     boost::shared_ptr<view::DsoSignal> dsoSig1,
                     boost::shared_ptr<view::DsoSignal> dsoSig2):
    Trace("M", dsoSig1->get_index(), SR_CHANNEL_MATH),
    _math_stack(math_stack),
    _dsoSig1(dsoSig1),
    _dsoSig2(dsoSig2),
    _enable(enable),
    _show(true),
    _scale(0),
    _zero_vrate(0.5),
    _hw_offset(0x80),
    _hover_en(false),
    _hover_index(0),
    _hover_point(QPointF(0, 0)),
    _hover_voltage(0)
{
    _vDial = _math_stack->get_vDial();
    update_vDial();
    _colour = View::Red;
    _ref_min = dsoSig1->get_ref_min();
    _ref_max = dsoSig1->get_ref_max();
}

MathTrace::~MathTrace()
{
}

bool MathTrace::enabled() const
{
    return _enable;
}

void MathTrace::set_enable(bool enable)
{
    _enable = enable;
}

int MathTrace::src1() const
{
    return _dsoSig1->get_index();
}

int MathTrace::src2() const
{
    return _dsoSig2->get_index();
}

float MathTrace::get_scale()
{
    return _scale;
}

int MathTrace::get_name_width() const
{
    return 0;
}

void MathTrace::update_vDial()
{
    _vDial->set_value(_math_stack->default_vDialValue());
}

void MathTrace::go_vDialPre()
{
    if (enabled() && !_vDial->isMin()) {
        if (_view->session().get_capture_state() == SigSession::Running)
            _view->session().refresh(DsoSignal::RefreshShort);
        const double pre_vdiv = _vDial->get_value();
        _vDial->set_sel(_vDial->get_sel() - 1);

        if (_view->session().get_capture_state() == SigSession::Stopped)
            _scale *= pre_vdiv/_vDial->get_value();

        _view->set_update(_viewport, true);
        _view->update();
    }
}

void MathTrace::go_vDialNext()
{
    if (enabled() && !_vDial->isMax()) {
        if (_view->session().get_capture_state() == SigSession::Running)
            _view->session().refresh(DsoSignal::RefreshShort);
        const double pre_vdiv = _vDial->get_value();
        _vDial->set_sel(_vDial->get_sel() + 1);

        if (_view->session().get_capture_state() == SigSession::Stopped)
            _scale *= pre_vdiv/_vDial->get_value();

        _view->set_update(_viewport, true);
        _view->update();
    }
}

uint64_t MathTrace::get_vDialValue() const
{
    return _vDial->get_value();
}

uint16_t MathTrace::get_vDialSel() const
{
    return _vDial->get_sel();
}

double MathTrace::get_zero_ratio()
{
    return _zero_vrate;
}

void MathTrace::set_zero_vrate(double rate)
{
    _zero_vrate = rate;
    _hw_offset = _zero_vrate * (_ref_max - _ref_min) + _ref_min;
}

int MathTrace::get_zero_vpos() const
{
    return _zero_vrate * get_view_rect().height() + DsoSignal::UpMargin;
}

void MathTrace::set_zero_vpos(int pos)
{
    if (enabled()) {
        set_zero_vrate(min(max(pos - DsoSignal::UpMargin, 0),
                           get_view_rect().height()) * 1.0 / get_view_rect().height());
    }
}

void MathTrace::set_show(bool show)
{
    _show = show;
}

QRect MathTrace::get_view_rect() const
{
    assert(_viewport);
    return QRect(0, DsoSignal::UpMargin,
                  _viewport->width() - DsoSignal::RightMargin,
                  _viewport->height() - DsoSignal::UpMargin - DsoSignal::DownMargin);
}

void MathTrace::paint_back(QPainter &p, int left, int right, QColor fore, QColor back)
{
    (void)p;
    (void)left;
    (void)right;
    (void)fore;
    (void)back;
}

void MathTrace::paint_mid(QPainter &p, int left, int right, QColor fore, QColor back)
{
    (void)fore;
    (void)back;

    if (!_show)
        return;

    assert(_math_stack);
    assert(_view);
    assert(right >= left);

    if (enabled()) {
        const float top = get_view_rect().top();
        const int height = get_view_rect().height();
        const int width = right - left;
        const float zeroY = _zero_vrate * height + top;

        const double scale = _view->scale();
        assert(scale > 0);
        const int64_t offset = _view->offset();

        const double pixels_offset = offset;
        //const double samplerate = _view->session().cur_snap_samplerate();
        const double samplerate = _math_stack->samplerate();
        const int64_t last_sample = max((int64_t)(_math_stack->get_sample_num() - 1), (int64_t)0);
        const double samples_per_pixel = samplerate * scale;
        const double start = offset * samples_per_pixel - _view->trig_hoff();
        const double end = start + samples_per_pixel * width;

        const int64_t start_sample = min(max((int64_t)floor(start),
            (int64_t)0), last_sample);
        const int64_t end_sample = min(max((int64_t)ceil(end) + 1,
            (int64_t)0), last_sample);

        _scale = get_view_rect().height() * _math_stack->get_math_scale() * 1000.0 / get_vDialValue();

        if (samples_per_pixel < DsoSignal::EnvelopeThreshold) {
            _math_stack->enable_envelope(false);
            paint_trace(p, zeroY, left,
                start_sample, end_sample,
                pixels_offset, samples_per_pixel);
        } else {
            _math_stack->enable_envelope(true);
            paint_envelope(p, zeroY, left,
                start_sample, end_sample,
                pixels_offset, samples_per_pixel);
        }
    }
}

void MathTrace::paint_fore(QPainter &p, int left, int right, QColor fore, QColor back)
{
    if (!_show)
        return;

    assert(_view);

    fore.setAlpha(View::BackAlpha);
    QPen pen(fore);
    pen.setStyle(Qt::DotLine);
    p.setPen(pen);
    p.drawLine(left, get_zero_vpos(), right, get_zero_vpos());

    // Paint measure
    fore.setAlpha(View::ForeAlpha);
    if (_view->session().get_capture_state() == SigSession::Stopped)
        paint_hover_measure(p, fore, back);
}

void MathTrace::paint_trace(QPainter &p,
    int zeroY, int left, const int64_t start, const int64_t end,
    const double pixels_offset, const double samples_per_pixel)
{
    const int64_t sample_count = end - start + 1;

    if (sample_count > 0) {
        QColor trace_colour = _colour;
        trace_colour.setAlpha(View::ForeAlpha);
        p.setPen(trace_colour);

        if ((uint64_t)end >= _math_stack->get_sample_num())
            return;

        const double *const values = _math_stack->get_math(start);
        assert(values);

        QPointF *points = new QPointF[sample_count];
        QPointF *point = points;

        double top = get_view_rect().top();
        double bottom = get_view_rect().bottom();
        float x = (start / samples_per_pixel - pixels_offset) + left + _view->trig_hoff()/samples_per_pixel;
        double  pixels_per_sample = 1.0/samples_per_pixel;

        for (int64_t index = 0; index < sample_count; index++) {
            const float y = min(max(top, zeroY - (values[index] * _scale)), bottom);
            if (x > get_view_rect().right()) {
                point--;
                const float lastY = point->y() + (y - point->y()) / (x - point->x()) * (get_view_rect().right() - point->x());
                point++;
                *point++ = QPointF(get_view_rect().right(), lastY);
                break;
            }
            *point++ = QPointF(x, y);
            x += pixels_per_sample;
        }

        p.drawPolyline(points, point - points);
        delete[] points;
    }
}

void MathTrace::paint_envelope(QPainter &p,
    int zeroY, int left, const int64_t start, const int64_t end,
    const double pixels_offset, const double samples_per_pixel)
{
	using namespace Qt;

    data::MathStack::EnvelopeSection e;
    _math_stack->get_math_envelope_section(e, start, end, samples_per_pixel);

	if (e.length < 2)
		return;

    p.setPen(QPen(NoPen));
    QColor envelope_colour = _colour;
    envelope_colour.setAlpha(View::ForeAlpha);
    p.setBrush(envelope_colour);

	QRectF *const rects = new QRectF[e.length];
	QRectF *rect = rects;
    double top = get_view_rect().top();
    double bottom = get_view_rect().bottom();
    for(uint64_t sample = 0; sample < e.length-1; sample++) {
		const float x = ((e.scale * sample + e.start) /
            samples_per_pixel - pixels_offset) + left + _view->trig_hoff()/samples_per_pixel;
        const data::MathStack::EnvelopeSample *const s =
			e.samples + sample;

		// We overlap this sample with the next so that vertical
		// gaps do not appear during steep rising or falling edges
        const float b = min(max(top, zeroY - max(s->max, (s+1)->min) * _scale), bottom);
        const float t = min(max(top, zeroY - min(s->min, (s+1)->max) * _scale), bottom);

		float h = b - t;
		if(h >= 0.0f && h <= 1.0f)
			h = 1.0f;
		if(h <= 0.0f && h >= -1.0f)
			h = -1.0f;

		*rect++ = QRectF(x, t, 1.0f, h);
	}

	p.drawRects(rects, e.length);

	delete[] rects;
}

void MathTrace::paint_type_options(QPainter &p, int right, const QPoint pt, QColor fore)
{
    p.setRenderHint(QPainter::Antialiasing, true);

    QColor foreBack = fore;
    foreBack.setAlpha(View::BackAlpha);
    int y = get_y();
    const QRectF vDial_rect = get_rect(DSO_VDIAL, y, right);

    QString pText;
    _vDial->paint(p, vDial_rect, _colour, pt, pText);
    QFontMetrics fm(p.font());
    const QRectF valueRect = QRectF(0, vDial_rect.top()-fm.height()-10, right, fm.height());
    p.drawText(valueRect, Qt::AlignCenter, pText);

    p.setRenderHint(QPainter::Antialiasing, false);
}

bool MathTrace::mouse_wheel(int right, const QPoint pt, const int shift)
{
    int y = get_y();
    const QRectF vDial_rect = get_rect(DSO_VDIAL, y, right);

    if (vDial_rect.contains(pt)) {
        if (shift > 0.5)
            go_vDialPre();
        else if (shift < -0.5)
            go_vDialNext();
        return true;
    } else {
        return false;
    }

    return true;
}

QRectF MathTrace::get_rect(MathSetRegions type, int y, int right)
{
    (void)right;

    if (type == DSO_VDIAL)
        return QRectF(
            get_leftWidth() + SquareWidth*0.5 + Margin,
            y - SquareWidth * SquareNum + SquareWidth * 3,
            SquareWidth * (SquareNum-1), SquareWidth * (SquareNum-1));
    else
        return QRectF(0, 0, 0, 0);
}

void MathTrace::paint_hover_measure(QPainter &p, QColor fore, QColor back)
{
    // Hover measure
    if (_hover_en) {
        QString hover_str = get_voltage(_hover_voltage, 2);
        const int hover_width = p.boundingRect(0, 0, INT_MAX, INT_MAX,
            Qt::AlignLeft | Qt::AlignTop, hover_str).width() + 10;
        const int hover_height = p.boundingRect(0, 0, INT_MAX, INT_MAX,
            Qt::AlignLeft | Qt::AlignTop, hover_str).height();
        QRectF hover_rect(_hover_point.x(), _hover_point.y()-hover_height/2, hover_width, hover_height);
        if (hover_rect.right() > get_view_rect().right())
            hover_rect.moveRight(_hover_point.x());
        if (hover_rect.top() < get_view_rect().top())
            hover_rect.moveTop(_hover_point.y());
        if (hover_rect.bottom() > get_view_rect().bottom())
            hover_rect.moveBottom(_hover_point.y());

        p.setPen(fore);
        p.setBrush(back);
        p.drawRect(_hover_point.x()-1, _hover_point.y()-1,
                   DsoSignal::HoverPointSize, DsoSignal::HoverPointSize);
        p.drawText(hover_rect, Qt::AlignCenter | Qt::AlignTop | Qt::TextDontClip, hover_str);
    }

    list<Cursor*>::iterator i = _view->get_cursorList().begin();
    while (i != _view->get_cursorList().end()) {
        float pt_value;
        const QPointF pt = get_point((*i)->index(), pt_value);
        QString pt_str = get_voltage(pt_value, 2);
        const int pt_width = p.boundingRect(0, 0, INT_MAX, INT_MAX,
            Qt::AlignLeft | Qt::AlignTop, pt_str).width() + 10;
        const int pt_height = p.boundingRect(0, 0, INT_MAX, INT_MAX,
            Qt::AlignLeft | Qt::AlignTop, pt_str).height();
        QRectF pt_rect(pt.x(), pt.y()-pt_height/2, pt_width, pt_height);
        if (pt_rect.right() > get_view_rect().right())
            pt_rect.moveRight(pt.x());
        if (pt_rect.top() < get_view_rect().top())
            pt_rect.moveTop(pt.y());
        if (pt_rect.bottom() > get_view_rect().bottom())
            pt_rect.moveBottom(pt.y());

        p.drawRect(pt.x()-1, pt.y()-1, 2, 2);
        p.drawLine(pt.x()-2, pt.y()-2, pt.x()+2, pt.y()+2);
        p.drawLine(pt.x()+2, pt.y()-2, pt.x()-2, pt.y()+2);
        p.drawText(pt_rect, Qt::AlignCenter | Qt::AlignTop | Qt::TextDontClip, pt_str);

        i++;
    }
}

bool MathTrace::measure(const QPointF &p)
{
    _hover_en = false;
    if (!enabled())
        return false;

    const QRectF window = get_view_rect();
    if (!window.contains(p))
        return false;

    _hover_index = _view->pixel2index(p.x());
    if (_hover_index >= _math_stack->get_sample_num())
        return false;

    _hover_point = get_point(_hover_index, _hover_voltage);
    _hover_en = true;
    return true;
}

QPointF MathTrace::get_point(uint64_t index, float &value)
{
    QPointF pt = QPointF(0, 0);

    const float top = get_view_rect().top();
    const float bottom = get_view_rect().bottom();
    const float zeroP = _zero_vrate * get_view_rect().height() + top;
    const float x = _view->index2pixel(index);

    value = *_math_stack->get_math(index);
    float y = min(max(top, zeroP - (value * _scale)), bottom);
    pt = QPointF(x, y);
    return pt;
}

QString MathTrace::get_voltage(double v, int p)
{
    return abs(v) >= 1 ? QString::number(v, 'f', p) + _math_stack->get_unit(1) :
                         QString::number(v * 1000, 'f', p) + _math_stack->get_unit(0);
}

QString MathTrace::get_time(double t)
{
    QString str = (abs(t) > 1000000000 ? QString::number(t/1000000000, 'f', 2) + "S" :
                   abs(t) > 1000000 ? QString::number(t/1000000, 'f', 2) + "mS" :
                   abs(t) > 1000 ? QString::number(t/1000, 'f', 2) + "uS" : QString::number(t, 'f', 2) + "nS");
    return str;
}

const boost::shared_ptr<pv::data::MathStack>& MathTrace::get_math_stack() const
{
   return _math_stack;
}


} // namespace view
} // namespace pv
