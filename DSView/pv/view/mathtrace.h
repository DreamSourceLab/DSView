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


#ifndef DSVIEW_PV_MATHTRACE_H
#define DSVIEW_PV_MATHTRACE_H

#include "trace.h" 

namespace pv {

namespace data {
class DsoSnapshot;
class MathStack;
}

namespace view {

class DsoSignal;

class MathTrace : public Trace
{
    Q_OBJECT

public:
    enum MathSetRegions {
        DSO_NONE = -1,
        DSO_VDIAL,
    };

public:
    MathTrace(bool enable, pv::data::MathStack *math_stack,
              view::DsoSignal *dsoSig1,
              view::DsoSignal *dsoSig2);

    virtual ~MathTrace();

    float get_scale();

    int get_name_width();

    /**
     *
     */
    void update_vDial();
    void go_vDialPre();
    void go_vDialNext();
    uint64_t get_vDialValue();
    uint16_t get_vDialSel();

    bool enabled();
    void set_enable(bool enable);
    void set_show(bool show);

    int get_zero_vpos();
    void set_zero_vpos(int pos);

    int src1();
    int src2();

    /**
      *
      */
    bool measure(const QPointF &p);
    QPointF get_point(uint64_t index, float &value);


    /**
     * Gets the mid-Y position of this signal.
     */
    double get_zero_ratio();

    /**
     * Sets the mid-Y position of this signal.
     */
    void set_zero_vrate(double rate);

    QString get_voltage(double v, int p);
    QString get_time(double t);

    /**
     * Paints the background layer of the trace with a QPainter
     * @param p the QPainter to paint into.
     * @param left the x-coordinate of the left edge of the signal
     * @param right the x-coordinate of the right edge of the signal
     **/
    void paint_back(QPainter &p, int left, int right, QColor fore, QColor back);

	/**
	 * Paints the signal with a QPainter
	 * @param p the QPainter to paint into.
	 * @param left the x-coordinate of the left edge of the signal.
	 * @param right the x-coordinate of the right edge of the signal.
	 **/
    void paint_mid(QPainter &p, int left, int right, QColor fore, QColor back);

    /**
     * Paints the signal with a QPainter
     * @param p the QPainter to paint into.
     * @param left the x-coordinate of the left edge of the signal.
     * @param right the x-coordinate of the right edge of the signal.
     **/
    void paint_fore(QPainter &p, int left, int right, QColor fore, QColor back);

    QRect get_view_rect();

    QRectF get_rect(MathSetRegions type, int y, int right);

    bool mouse_wheel(int right, const QPoint pt, const int shift);

    pv::data::MathStack* get_math_stack();

protected:
    void paint_type_options(QPainter &p, int right, const QPoint pt, QColor fore);

private:
    void paint_trace(QPainter &p,
        int zeroY, int left, const int64_t start, const int64_t end,
        const double pixels_offset, const double samples_per_pixel);

    void paint_envelope(QPainter &p,
        int zeroY, int left, const int64_t start, const int64_t end,
        const double pixels_offset, const double samples_per_pixel);

    void paint_hover_measure(QPainter &p, QColor fore, QColor back);

private:
    pv::data::MathStack *_math_stack;
    view::DsoSignal *_dsoSig1;
    view::DsoSignal *_dsoSig2;
    bool _enable;
    bool _show;

    dslDial *_vDial;
    double _ref_min;
    double _ref_max;
    float _scale;

    double _zero_vrate;
    float _hw_offset;

    bool _hover_en;
    uint64_t _hover_index;
    QPointF _hover_point;
    float _hover_voltage;
};

} // namespace view
} // namespace pv

#endif // DSVIEW_PV_MATHTRACE_H
