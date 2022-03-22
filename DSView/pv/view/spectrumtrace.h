/*
 * This file is part of the PulseView project.
 * DSView is based on PulseView.
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

#ifndef DSVIEW_PV_VIEW_SPECTRUMTRACE_H
#define DSVIEW_PV_VIEW_SPECTRUMTRACE_H

#include "trace.h"

#include <list>
#include <map>
 

struct srd_channel;

namespace pv {

class SigSession;

namespace data{
class SpectrumStack;
}

namespace view {

//when device is oscillcopse mode, to draw signal trace
//created by SigSession
class SpectrumTrace : public Trace
{
    Q_OBJECT

private:
    static const int UpMargin;
    static const int DownMargin;
    static const int RightMargin;
    static const QString FFT_ViewMode[2];

    static const QString FreqPrefixes[9];
    static const int FirstSIPrefixPower;
    static const int LastSIPrefixPower;
    static const int Pricision;
    static const int FreqMinorDivNum;
    static const int TickHeight;
    static const int VolDivNum;

    static const int DbvRanges[4];

    static const int HoverPointSize;

    static const double VerticalRate;

public:
    SpectrumTrace(pv::SigSession *session, pv::data::SpectrumStack *spectrum_stack, int index);
    ~SpectrumTrace();

    bool enabled();
    void set_enable(bool enable);

    void init_zoom();
    void zoom(double steps, int offset);
    bool zoom_hit();
    void set_zoom_hit(bool hit);

    void set_offset(double delta);
    double get_offset();

    void set_scale(double scale);
    double get_scale();

    void set_dbv_range(int range);
    int dbv_range();
    std::vector<int> get_dbv_ranges();

    int view_mode();
    void set_view_mode(unsigned int mode);
    std::vector<QString> get_view_modes_support();

    pv::data::SpectrumStack* get_spectrum_stack();

    static QString format_freq(double freq, unsigned precision = Pricision);

    bool measure(const QPoint &p);

    /**
     * Paints the background layer of the trace with a QPainter
     * @param p the QPainter to paint into.
     * @param left the x-coordinate of the left edge of the signal.
     * @param right the x-coordinate of the right edge of the signal.
     **/
    void paint_back(QPainter &p, int left, int right, QColor fore, QColor back);

    /**
     * Paints the mid-layer of the trace with a QPainter
     * @param p the QPainter to paint into.
     * @param left the x-coordinate of the left edge of the signal
     * @param right the x-coordinate of the right edge of the signal
     **/
    void paint_mid(QPainter &p, int left, int right, QColor fore, QColor back);

    /**
     * Paints the foreground layer of the trace with a QPainter
     * @param p the QPainter to paint into.
     * @param left the x-coordinate of the left edge of the signal
     * @param right the x-coordinate of the right edge of the signal
     **/
    void paint_fore(QPainter &p, int left, int right, QColor fore, QColor back);

    QRect get_view_rect();

protected:
    void paint_type_options(QPainter &p, int right, const QPoint pt, QColor fore);

private:

private slots:

private:
    pv::SigSession *_session;
    pv::data::SpectrumStack *_spectrum_stack;

    bool _enable;
    int _view_mode;

    double _vmax;
    double _vmin;
    int _dbv_range;

    uint64_t _hover_index;
    bool _hover_en;
    QPointF _hover_point;
    double _hover_value;

    double _scale;
    double _offset;
};

} // namespace view
} // namespace pv

#endif // DSVIEW_PV_VIEW_SPECTRUMTRACE_H
