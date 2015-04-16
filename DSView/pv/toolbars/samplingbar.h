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


#ifndef DSLOGIC_PV_TOOLBARS_SAMPLINGBAR_H
#define DSLOGIC_PV_TOOLBARS_SAMPLINGBAR_H

#include <stdint.h>

#include <list>
#include <map>

#include <boost/shared_ptr.hpp>

#include <QComboBox>
#include <QToolBar>
#include <QToolButton>

#include <libsigrok4DSLogic/libsigrok.h>

#include "../sigsession.h"

struct st_dev_inst;
class QAction;

namespace pv {

class SigSession;

namespace device {
class DevInst;
}

namespace dialogs {
class deviceoptions;
}

namespace toolbars {

class SamplingBar : public QToolBar
{
	Q_OBJECT

private:
    static const uint64_t RecordLengths[19];
    static const uint64_t DefaultRecordLength;
    static const uint64_t DSLogic_RecordLengths[15];
    static const uint64_t DSLogic_DefaultRecordLength;

public:
    SamplingBar(SigSession &session, QWidget *parent);

    void set_device_list(const std::list< boost::shared_ptr<pv::device::DevInst> > &devices,
                         boost::shared_ptr<pv::device::DevInst> selected);

    boost::shared_ptr<pv::device::DevInst> get_selected_device() const;

	uint64_t get_record_length() const;
    void set_record_length(uint64_t length);

	void set_sampling(bool sampling);

    void enable_toggle(bool enable);

    void enable_run_stop(bool enable);

    void enable_instant(bool enable);

public slots:
    void set_sample_rate(uint64_t sample_rate);
    void set_sample_limit(uint64_t sample_limit);

signals:
	void run_stop();
    void instant_stop();
    void device_selected();
    void device_updated();
    void update_scale();

private:
    void update_sample_rate_selector();
	void update_sample_rate_selector_value();
    void update_sample_count_selector();
    void update_sample_count_selector_value();
	void commit_sample_rate();
    void commit_sample_count();

private slots:
	void on_run_stop();
    void on_instant_stop();
    void on_device_selected();
    void on_samplerate_sel(int index);

public slots:
    void on_configure();

private:
    SigSession &_session;

    bool _enable;

    QComboBox _device_selector;
    std::map<const void*, boost::weak_ptr<device::DevInst> >
        _device_selector_map;
    bool _updating_device_selector;

    QToolButton _configure_button;

    QComboBox _sample_count;
    QComboBox _sample_rate;
    bool _updating_sample_rate;
    bool _updating_sample_count;

    QIcon _icon_stop;
    QIcon _icon_start;
    QIcon _icon_instant;
	QToolButton _run_stop_button;
    QToolButton _instant_button;

    bool _instant;
};

} // namespace toolbars
} // namespace pv

#endif // DSLOGIC_PV_TOOLBARS_SAMPLINGBAR_H
