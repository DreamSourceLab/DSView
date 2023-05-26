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

#include "spectrumstack.h"
 
#include "dsosnapshot.h"
#include "../sigsession.h"
#include "../view/dsosignal.h"
#include <math.h>

#define PI 3.1415

using namespace boost;
using namespace std;

namespace pv {
namespace data {

const QString SpectrumStack::windows_support[5] = {
    QT_TR_NOOP("Rectangle"),
    QT_TR_NOOP("Hann"),
    QT_TR_NOOP("Hamming"),
    QT_TR_NOOP("Blackman"),
    QT_TR_NOOP("Flat_top")
};

const uint64_t SpectrumStack::length_support[5] = {
    1024,
    2048,
    4096,
    8192,
    16384,
};

SpectrumStack::SpectrumStack(pv::SigSession *session, int index) :
    _session(session),
    _index(index),
    _dc_ignore(true),
    _sample_interval(1),
    _spectrum_state(Init),
    _fft_plan(NULL)
{
}

SpectrumStack::~SpectrumStack()
{
    _xn.clear();
    _xk.clear();
    _power_spectrum.clear();
    if (_fft_plan)
        fftw_destroy_plan(_fft_plan);
}

void SpectrumStack::clear()
{
}

void SpectrumStack::init()
{
}

int SpectrumStack::get_index()
{
    return _index;
}

uint64_t SpectrumStack::get_sample_num()
{
    return _sample_num;
}

void SpectrumStack::set_sample_num(uint64_t num)
{
    _sample_num = num;
    _xn.resize(_sample_num);
    _xk.resize(_sample_num);
    _power_spectrum.resize(_sample_num/2+1);
    _fft_plan = fftw_plan_r2r_1d(_sample_num, _xn.data(), _xk.data(),
                                 FFTW_R2HC, FFTW_ESTIMATE);
}

int SpectrumStack::get_windows_index()
{
    return _windows_index;
}

void SpectrumStack::set_windows_index(int index)
{
    _windows_index = index;
}

bool SpectrumStack::dc_ignored()
{
    return _dc_ignore;
}

void SpectrumStack::set_dc_ignore(bool ignore)
{
    _dc_ignore = ignore;
}

int SpectrumStack::get_sample_interval()
{
    return _sample_interval;
}

void SpectrumStack::set_sample_interval(int interval)
{
    _sample_interval = interval;
}

const std::vector<QString> SpectrumStack::get_windows_support()
{
    std::vector<QString> windows;
    for (size_t i = 0; i < sizeof(windows_support)/sizeof(windows_support[0]); i++)
    {
        windows.push_back(windows_support[i]);
    }
    return windows;
}

const std::vector<uint64_t> SpectrumStack::get_length_support()
{
    std::vector<uint64_t> length;
    for (size_t i = 0; i < sizeof(length_support)/sizeof(length_support[0]); i++)
    {
        length.push_back(length_support[i]);
    }
    return length;
}

const std::vector<double> SpectrumStack::get_fft_spectrum()
{
    std::vector<double> empty;
    if (_spectrum_state == Stopped)
        return _power_spectrum;
    else
        return empty;
}

double SpectrumStack::get_fft_spectrum(uint64_t index)
{
    double ret = -1;
    if (_spectrum_state == Stopped && index < _power_spectrum.size())
        ret = _power_spectrum[index];

    return ret;
}

void SpectrumStack::calc_fft()
{
    _spectrum_state = Running;
    // Get the dso data
    pv::data::DsoSnapshot *data = NULL;
    pv::view::DsoSignal *dsoSig = NULL;

    for(auto s : _session->get_signals()) {
        if (s->signal_type() == SR_CHANNEL_DSO) {
            dsoSig = (view::DsoSignal*)s;
            if (dsoSig->get_index() == _index && dsoSig->enabled()) {
                data = dsoSig->data();
                break;
            }
        }
    }

    if (data == NULL || data->empty())
        return;

    if (data->get_sample_count() < _sample_num * _sample_interval)
        return;

    // Get the samplerate
    _samplerate = data->samplerate();
    if (_samplerate == 0.0)
        _samplerate = 1.0;

    // prepare _xn data
    const int offset = dsoSig->get_hw_offset();
    const double vscale = dsoSig->get_vDialValue() * dsoSig->get_factor() * DS_CONF_DSO_VDIVS / (1000*255.0);
    const uint16_t step = _sample_interval;
    const uint8_t *const samples = data->get_samples(0, _sample_num*_sample_interval-1, _index);
    double wsum = 0;
    
    for (unsigned int i = 0; i < _sample_num; i++) {
        double w = window(i, _windows_index);
        _xn[i] = (samples[i*step] - offset) * vscale * w;
        wsum += w;
    }

    // fft
    fftw_execute(_fft_plan);

    // calculate power spectrum
    _power_spectrum[0] = abs(_xk[0])/wsum;  /* DC component */
    for (unsigned int k = 1; k < (_sample_num + 1) / 2; ++k)  /* (k < N/2 rounded up) */
         _power_spectrum[k] = sqrt((_xk[k]*_xk[k] + _xk[_sample_num-k]*_xk[_sample_num-k]) * 2) / wsum;
    if (_sample_num % 2 == 0) /* N is even */
         _power_spectrum[_sample_num/2] = abs(_xk[_sample_num/2])/wsum;  /* Nyquist freq. */

    _spectrum_state = Stopped;
}

double SpectrumStack::window(uint64_t i, int type)
{
    const double n_m_1 = _sample_num-1;
    switch(type) {
    case 1: // Hann window
        return 0.5*(1-cos(2*PI*i/n_m_1));
    case 2: // Hamming window
        return 0.54-0.46*cos(2*PI*i/n_m_1);
    case 3: // Blackman window
        return 0.42659-0.49656*cos(2*PI*i/n_m_1) + 0.076849*cos(4*PI*i/n_m_1);
    case 4: // Flat_top window
        return 1-1.93*cos(2*PI*i/n_m_1)+1.29*cos(4*PI*i/n_m_1)-
                 0.388*cos(6*PI*i/n_m_1)+0.028*cos(8*PI*i/n_m_1);
    default:
        return 1;
    }
}

} // namespace data
} // namespace pv
