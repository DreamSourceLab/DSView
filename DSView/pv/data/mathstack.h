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

#ifndef DSVIEW_PV_DATA_MATHSTACK_H
#define DSVIEW_PV_DATA_MATHSTACK_H

#include "signaldata.h"

#include <list>

#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>

#include <fftw3.h>

#include <QObject>
#include <QString>

namespace pv {

class SigSession;

namespace view {
class DsoSignal;
}

namespace data {

class DsoSnapshot;
class Dso;

class MathStack : public QObject, public SignalData
{
    Q_OBJECT

private:
    static const QString windows_support[5];
    static const uint64_t length_support[5];

public:
    enum math_state {
        Init,
        Stopped,
        Running
    };

public:
    MathStack(pv::SigSession &_session, int index);
    virtual ~MathStack();
    void clear();
    void init();

    int get_index() const;

    uint64_t get_sample_num() const;
    void set_sample_num(uint64_t num);

    int get_windows_index() const;
    void set_windows_index(int index);

    const std::vector<QString> get_windows_support() const;
    const std::vector<uint64_t> get_length_support() const;

    bool dc_ignored() const;
    void set_dc_ignore(bool ignore);

    int get_sample_interval() const;
    void set_sample_interval(int interval);

    const std::vector<double> get_fft_spectrum() const;
    const double get_fft_spectrum(uint64_t index) const;

    void calc_fft();

    double window(uint64_t i, int type);

signals:

private:
    pv::SigSession &_session;

    int _index;
    uint64_t _sample_num;
    int _windows_index;
    bool _dc_ignore;
    int _sample_interval;

    boost::shared_ptr<pv::data::DsoSnapshot> _snapshot;

    std::unique_ptr<boost::thread> _math_thread;
    math_state _math_state;

    fftw_plan _fft_plan;
    std::vector<double> _xn;
    std::vector<double> _xk;
    std::vector<double> _power_spectrum;
};

} // namespace data
} // namespace pv

#endif // DSVIEW_PV_DATA_MATHSTACK_H
