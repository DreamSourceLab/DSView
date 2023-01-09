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

#ifndef DSVIEW_PV_DATA_MATHSTACK_H
#define DSVIEW_PV_DATA_MATHSTACK_H

#include "signaldata.h"

#include <list>

#include <boost/optional.hpp> 
  

#include <QObject>
#include <QString>

namespace pv {

class SigSession;

namespace view {
class DsoSignal;
class dslDial;
}

namespace data {

class DsoSnapshot;

class MathStack : public QObject, public SignalData
{
    Q_OBJECT

public:
    enum math_state {
        Init,
        Stopped,
        Running
    };

    enum MathType {
        MATH_ADD,
        MATH_SUB,
        MATH_MUL,
        MATH_DIV,
    };

    struct EnvelopeSample
    {
        double min;
        double max;
    };

    struct EnvelopeSection
    {
        uint64_t start;
        unsigned int scale;
        uint64_t length;
        EnvelopeSample *samples;
    };

private:
    struct Envelope
    {
        uint64_t length;
        uint64_t data_length;
        EnvelopeSample *samples;
    };

private:
    static const unsigned int ScaleStepCount = 10;
    static const int EnvelopeScalePower;
    static const int EnvelopeScaleFactor;
    static const float LogEnvelopeScaleFactor;
    static const uint64_t EnvelopeDataUnit;

    static const uint64_t vDialValueStep = 1000;
    static const int vDialValueCount = 19;
    static const uint64_t vDialValue[vDialValueCount];
    static const int vDialUnitCount = 2;
    static const QString vDialAddUnit[vDialUnitCount];
    static const QString vDialMulUnit[vDialUnitCount];
    static const QString vDialDivUnit[vDialUnitCount];

public:
    MathStack(pv::SigSession *_session,
              view::DsoSignal *dsoSig1,
              view::DsoSignal *dsoSig2, MathType type);
    virtual ~MathStack();
    void clear();
    void init();
    void free_envelop();
    void realloc(uint64_t num);

    MathType get_type();
    uint64_t get_sample_num();

    void enable_envelope(bool enable);

    uint64_t default_vDialValue();
    view::dslDial *get_vDial();
    QString get_unit(int level);
    double get_math_scale();

    const double *get_math(uint64_t start);
    void get_math_envelope_section(EnvelopeSection &s,
        uint64_t start, uint64_t end, float min_length);

    void calc_math();
    void reallocate_envelope(Envelope &e);
    void append_to_envelope_level(bool header);

signals:

private:
    pv::SigSession  *_session;
    view::DsoSignal *_dsoSig1;
    view::DsoSignal *_dsoSig2;

    MathType _type;
    uint64_t _sample_num;
    uint64_t _total_sample_num;
    math_state _math_state;

    struct Envelope _envelope_level[ScaleStepCount];
    std::vector<double> _math;

    bool _envelope_en;
    bool _envelope_done;
};

} // namespace data
} // namespace pv

#endif // DSVIEW_PV_DATA_MATHSTACK_H
