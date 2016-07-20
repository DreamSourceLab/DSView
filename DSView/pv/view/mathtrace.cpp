/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2016 DreamSourceLab <support@dreamsourcelab.com>
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
#include "mathtrace.h"

#include <algorithm>
#include <math.h>

#include <boost/foreach.hpp>
#include <boost/functional/hash.hpp>

#include "../sigsession.h"
#include "../data/dso.h"
#include "../data/dsosnapshot.h"
#include "../view/dsosignal.h"
#include "../view/viewport.h"
#include "../device/devinst.h"

#include "../data/mathstack.h"

using namespace boost;
using namespace std;

namespace pv {
namespace view {

const int MathTrace::UpMargin = 0;
const int MathTrace::DownMargin = 0;
const int MathTrace::RightMargin = 30;
const QString MathTrace::FFT_ViewMode[2] = {
    "Linear RMS",
    "DBV RMS"
};

const QString MathTrace::FreqPrefixes[9] =
    {"", "", "", "", "K", "M", "G", "T", "P"};
const int MathTrace::FirstSIPrefixPower = -9;
const int MathTrace::LastSIPrefixPower = 15;
const int MathTrace::Pricision = 2;
const int MathTrace::FreqMinorDivNum = 10;
const int MathTrace::TickHeight = 15;
const int MathTrace::VolDivNum = 5;

const int MathTrace::DbvRanges[4] = {
    100,
    120,
    150,
    200,
};

const int MathTrace::HoverPointSize = 3;
const double MathTrace::VerticalRate = 1.0 / 2000.0;

MathTrace::MathTrace(pv::SigSession &session,
    boost::shared_ptr<pv::data::MathStack> math_stack, int index) :
    Trace("FFT("+QString::number(index)+")", index, SR_CHANNEL_FFT),
    _session(session),
    _math_stack(math_stack),
    _enable(false),
    _view_mode(0),
    _hover_en(false),
    _scale(1),
    _offset(0)
{
    const vector< boost::shared_ptr<Signal> > sigs(_session.get_signals());
    for(size_t i = 0; i < sigs.size(); i++) {
        const boost::shared_ptr<view::Signal> s(sigs[i]);
        assert(s);
        if (dynamic_pointer_cast<DsoSignal>(s) && index == s->get_index())
            _colour = s->get_colour();
    }
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

int MathTrace::view_mode() const
{
    return _view_mode;
}

void MathTrace::set_view_mode(int mode)
{
    assert(mode < sizeof(FFT_ViewMode)/sizeof(FFT_ViewMode[0]));
    _view_mode = mode;
}

std::vector<QString> MathTrace::get_view_modes_support()
{
    std::vector<QString> modes;
    for (unsigned int i = 0; i < sizeof(FFT_ViewMode)/sizeof(FFT_ViewMode[0]); i++) {
        modes.push_back(FFT_ViewMode[i]);
    }
    return modes;
}

const boost::shared_ptr<pv::data::MathStack>& MathTrace::get_math_stack() const
{
    return _math_stack;
}

void MathTrace::init_zoom()
{
    _scale = 1;
    _offset = 0;
}

void MathTrace::zoom(double steps, int offset)
{
    if (!_view)
        return;

    const int width = get_view_rect().width();
    double pre_offset = _offset + _scale*offset/width;
    _scale *= std::pow(3.0/2.0, -steps);
    _scale = max(min(_scale, 1.0), 100.0/_math_stack->get_sample_num());
    _offset = pre_offset - _scale*offset/width;
    _offset = max(min(_offset, 1-_scale), 0.0);

    _view->set_update(_viewport, true);
    _view->update();
}

void MathTrace::set_offset(double delta)
{
    int width = get_view_rect().width();
    _offset = _offset + (delta*_scale / width);
    _offset = max(min(_offset, 1-_scale), 0.0);

    _view->set_update(_viewport, true);
    _view->update();
}

double MathTrace::get_offset() const
{
    return _offset;
}

void MathTrace::set_scale(double scale)
{
    _scale = max(min(scale, 1.0), 100.0/_math_stack->get_sample_num());

    _view->set_update(_viewport, true);
    _view->update();
}

double MathTrace::get_scale() const
{
    return _scale;
}

void MathTrace::set_dbv_range(int range)
{
    _dbv_range = range;
}

int MathTrace::dbv_range() const
{
    return _dbv_range;
}

std::vector<int> MathTrace::get_dbv_ranges()
{
    std::vector<int> range;
    for (unsigned int i = 0; i < sizeof(DbvRanges)/sizeof(DbvRanges[0]); i++) {
        range.push_back(DbvRanges[i]);
    }
    return range;
}

QString MathTrace::format_freq(double freq, unsigned precision)
{
    if (freq <= 0) {
        return "0Hz";
    } else {
        const int order = floor(log10f(freq));
        assert(order >= FirstSIPrefixPower);
        assert(order <= LastSIPrefixPower);
        const int prefix = floor((order - FirstSIPrefixPower)/ 3.0f);
        const double divider = pow(10.0, max(prefix * 3.0 + FirstSIPrefixPower, 0.0));

        QString s;
        QTextStream ts(&s);
        ts.setRealNumberPrecision(precision);
        ts << fixed << freq / divider <<
            FreqPrefixes[prefix] << "Hz";
        return s;
    }
}

bool MathTrace::measure(const QPoint &p)
{
    _hover_en = false;
    if(!_view || !enabled())
        return false;

    const QRect window = get_view_rect();
    if (!window.contains(p))
        return false;

    const std::vector<double> samples(_math_stack->get_fft_spectrum());
    if(samples.empty())
        return false;

    const unsigned int full_size = (_math_stack->get_sample_num()/2);
    const double view_off = full_size * _offset;
    const double view_size = full_size*_scale;
    const double sample_per_pixels = view_size/window.width();
    _hover_index = std::round(p.x() * sample_per_pixels + view_off);

    if (_hover_index < full_size)
        _hover_en = true;

    //_view->set_update(_viewport, true);
    _view->update();
    return true;
}


void MathTrace::paint_back(QPainter &p, int left, int right)
{
    if(!_view)
        return;

    const int height = get_view_rect().height();
    const int width = right - left;

    QPen solidPen(Signal::dsFore);
    solidPen.setStyle(Qt::SolidLine);
    p.setPen(solidPen);
    p.setBrush(Trace::dsBack);
    p.drawRect(left, UpMargin, width, height);
}

void MathTrace::paint_mid(QPainter &p, int left, int right)
{
    if(!_view)
        return;
    assert(right >= left);

    if (enabled()) {
        const std::vector<double> samples(_math_stack->get_fft_spectrum());
        if(samples.empty())
            return;

        QColor trace_colour = _colour;
        trace_colour.setAlpha(150);
        p.setPen(trace_colour);

        const int full_size = (_math_stack->get_sample_num()/2);
        const double view_off = full_size * _offset;
        const int view_start = floor(view_off);
        const int view_size = full_size*_scale;
        QPointF *points = new QPointF[samples.size()];
        QPointF *point = points;

        const bool dc_ignored = _math_stack->dc_ignored();
        const double height = get_view_rect().height();
        const double width = right - left;
        const double pixels_per_sample = width/view_size;

        double vdiv = 0;
        double vfactor = 0;
        BOOST_FOREACH(const boost::shared_ptr<Signal> s, _session.get_signals()) {
            boost::shared_ptr<DsoSignal> dsoSig;
            if (dsoSig = dynamic_pointer_cast<DsoSignal>(s)) {
                if(dsoSig->get_index() == _math_stack->get_index()) {
                    vdiv = dsoSig->get_vDialValue();
                    vfactor = dsoSig->get_factor();
                    break;
                }
            }
        }
        if (_view_mode == 0) {
            _vmin = 0;
            _vmax = (vdiv*DS_CONF_DSO_HDIVS*vfactor)*VerticalRate;
        } else {
            _vmax = 20*log10((vdiv*DS_CONF_DSO_HDIVS*vfactor)*VerticalRate);
            _vmin = _vmax - _dbv_range;
        }

        //const double max_value = *std::max_element(dc_ignored ? ++samples.begin() : samples.begin(), samples.end());
        //const double min_value = *std::min_element(dc_ignored ? ++samples.begin() : samples.begin(), samples.end());
        //_vmax = (_view_mode == 0) ? max_value : 20*log10(max_value);
        //_vmin = (_view_mode == 0) ? min_value : 20*log10(min_value);
        const double scale = height / (_vmax - _vmin);

        double x = (view_start-view_off)*pixels_per_sample;
        uint64_t sample = view_start;
        if (dc_ignored && sample == 0) {
            sample++;
            x += pixels_per_sample;
        }
        double min_mag = pow(10.0, _vmin/20);
        do{
            double mag = samples[sample];
            if (_view_mode != 0) {
                if (mag < min_mag)
                    mag = _vmin;
                else
                    mag = 20*log10(mag);
            }
            const double y = height - (scale * (mag - _vmin));
            *point++ = QPointF(x, y);
            x += pixels_per_sample;
            sample++;
        }while(x<right && sample < samples.size());

        p.drawPolyline(points, point - points);
        delete[] points;
    }
}

void MathTrace::paint_fore(QPainter &p, int left, int right)
{
    using namespace Qt;

    if(!_view)
        return;
    assert(right >= left);

    (void)left;
    (void)right;
    const int text_height = p.boundingRect(0, 0, INT_MAX, INT_MAX,
        AlignLeft | AlignTop, "8").height();
    const double width = get_view_rect().width();
    const double height = get_view_rect().height();
    double blank_top = 0;
    double blank_right = width;

    // horizontal ruler
    const double NyFreq = _session.get_device()->get_sample_rate() / (2.0 * _math_stack->get_sample_interval());
    const double deltaFreq = _session.get_device()->get_sample_rate() * 1.0 /
                            (_math_stack->get_sample_num() * _math_stack->get_sample_interval());
    const double FreqRange = NyFreq * _scale;
    const double FreqOffset = NyFreq * _offset;

    const int order = (int)floor(log10(FreqRange));
    const double multiplier = (pow(10.0, order) == FreqRange) ? FreqRange/10 : pow(10.0, order);
    const double freq_per_pixel = FreqRange / width;

    p.setPen(Trace::DARK_FORE);
    p.setBrush(Qt::NoBrush);
    double tick_freq = multiplier * (int)floor(FreqOffset / multiplier);
    int division = (int)round(tick_freq * FreqMinorDivNum / multiplier);
    double x = (tick_freq - FreqOffset) / freq_per_pixel;
    do{
        if (division%FreqMinorDivNum == 0) {
            QString freq_str = format_freq(tick_freq);
            double typical_width = p.boundingRect(0, 0, INT_MAX, INT_MAX,
                AlignLeft | AlignTop, freq_str).width() + 10;
            p.drawLine(x, 1, x, TickHeight);
            if (x > typical_width/2 && (width-x) > typical_width/2)
                p.drawText(x-typical_width/2, TickHeight, typical_width, text_height,
                           AlignCenter | AlignTop | TextDontClip, freq_str);
        } else {
                p.drawLine(x, 1, x, TickHeight/2);
        }
        tick_freq += multiplier/FreqMinorDivNum;
        division++;
        x =  (tick_freq - FreqOffset) / freq_per_pixel;
    } while(x < width);
    blank_top = max(blank_top, (double)TickHeight + text_height);

    // delta Frequency
    QString freq_str =  QString::fromWCharArray(L" \u0394") + "Freq: " + format_freq(deltaFreq,4);
    p.drawText(0, 0, width, get_view_rect().height(),
               AlignRight | AlignBottom | TextDontClip, freq_str);
    double delta_left = width-p.boundingRect(0, 0, INT_MAX, INT_MAX,
                                             AlignLeft | AlignTop, freq_str).width();
    blank_right = min(delta_left, blank_right);

    // Vertical ruler
    const double vRange = _vmax - _vmin;
    const double vOffset = _vmin;
    const double vol_per_tick = vRange / VolDivNum;

    p.setPen(Trace::DARK_FORE);
    p.setBrush(Qt::NoBrush);
    double tick_vol = vol_per_tick + vOffset;
    double y = height - height / VolDivNum;
    const QString unit = (_view_mode == 0) ? "" : "dbv";
    do{
        if (y > text_height && y < (height - text_height)) {
            QString vol_str = QString::number(tick_vol, 'f', Pricision) + unit;
            double vol_width = p.boundingRect(0, 0, INT_MAX, INT_MAX,
                AlignLeft | AlignTop, vol_str).width();
            p.drawLine(width, y, width-TickHeight/2, y);
            p.drawText(width-TickHeight-vol_width, y-text_height/2, vol_width, text_height,
                       AlignCenter | AlignTop | TextDontClip, vol_str);
            blank_right = min(width-TickHeight-vol_width, blank_right);
        }
        tick_vol += vol_per_tick;
        y -=  height / VolDivNum;
    } while(y > 0);

    // Hover measure
    if (_hover_en) {
        const std::vector<double> samples(_math_stack->get_fft_spectrum());
        if(samples.empty())
            return;
        const int full_size = (_math_stack->get_sample_num()/2);
        const double view_off = full_size * _offset;
        const int view_size = full_size*_scale;
        const double scale = height / (_vmax - _vmin);
        const double pixels_per_sample = width/view_size;
        double x = (_hover_index-view_off)*pixels_per_sample;
        double min_mag = pow(10.0, _vmin/20);
        _hover_value = samples[_hover_index];
        if (_view_mode != 0) {
            if (_hover_value < min_mag)
                _hover_value = _vmin;
            else
                _hover_value = 20*log10(_hover_value);
        }
        const double y = height - (scale * (_hover_value - _vmin));
        _hover_point = QPointF(x, y);

        p.setPen(QPen(Trace::DARK_FORE, 1, Qt::DashLine));
        p.setBrush(Qt::NoBrush);
        p.drawLine(_hover_point.x(), 0, _hover_point.x(), height);

        QString hover_str = QString::number(_hover_value, 'f', 4) + unit + "@" + format_freq(deltaFreq * _hover_index, 4);
        const int hover_width = p.boundingRect(0, 0, INT_MAX, INT_MAX,
            AlignLeft | AlignTop, hover_str).width();
        const int hover_height = p.boundingRect(0, 0, INT_MAX, INT_MAX,
            AlignLeft | AlignTop, hover_str).height();
        QRectF hover_rect(_hover_point.x(), _hover_point.y()-hover_height, hover_width, hover_height);
        if (hover_rect.right() > blank_right)
            hover_rect.moveRight(min(_hover_point.x(), blank_right));
        if (hover_rect.top() < blank_top)
            hover_rect.moveTop(max(_hover_point.y(), blank_top));
        if (hover_rect.top() > 0)
            p.drawText(hover_rect, AlignCenter | AlignTop | TextDontClip, hover_str);

        p.setPen(Qt::NoPen);
        p.setBrush(Trace::DARK_FORE);
        p.drawEllipse(_hover_point, HoverPointSize, HoverPointSize);
    }
}

void MathTrace::paint_type_options(QPainter &p, int right, const QPoint pt)
{
    (void)p;
    (void)pt;
    (void)right;
}

QRect MathTrace::get_view_rect() const
{
    assert(_viewport);
    return QRect(0, UpMargin,
                  _viewport->width() - RightMargin,
                  _viewport->height() - UpMargin - DownMargin);
}

} // namespace view
} // namespace pv
