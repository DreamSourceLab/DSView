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


#ifndef DSVIEW_PV_TOOLBARS_SAMPLINGBAR_H
#define DSVIEW_PV_TOOLBARS_SAMPLINGBAR_H

#include <stdint.h>

#include <list>
#include <map>

#include <boost/shared_ptr.hpp>

#include <QComboBox>
#include <QToolBar>
#include <QToolButton>

#include <libsigrok4DSL/libsigrok.h>

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
class Calibration;
}

namespace toolbars {

class SamplingBar : public QToolBar
{
	Q_OBJECT

private:
    static const uint64_t RecordLengths[19];
    static const uint64_t DefaultRecordLength;
    static const int ComboBoxMaxWidth = 200;

public:
    SamplingBar(SigSession &session, QWidget *parent);

    void set_device_list(const std::list< boost::shared_ptr<pv::device::DevInst> > &devices,
                         boost::shared_ptr<pv::device::DevInst> selected);

    boost::shared_ptr<pv::device::DevInst> get_selected_device() const;

	uint64_t get_record_length() const;
    void set_record_length(uint64_t length);
    void update_record_length();

    void update_sample_rate();

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
    void sample_count_changed();
    void show_calibration();
    void hide_calibration();

private:
    void update_sample_rate_selector();
	void update_sample_rate_selector_value();
    void update_sample_count_selector();
    void update_sample_count_selector_value();
	void commit_sample_rate();
    void commit_sample_count();
    void setting_adj();

private slots:
	void on_run_stop();
    void on_instant_stop();
    void on_device_selected();
    void on_samplerate_sel(int index);
    void on_samplecount_sel(int index);

    void show_session_error(
        const QString text, const QString info_text);

public slots:
    void on_configure();
    void zero_adj();
    void reload();

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
    QIcon _icon_start_dis;
    QIcon _icon_instant_dis;
	QToolButton _run_stop_button;
    QToolButton _instant_button;
    QAction* _run_stop_action;
    QAction* _instant_action;

    bool _instant;
};

} // namespace toolbars
} // namespace pv

#endif // DSVIEW_PV_TOOLBARS_SAMPLINGBAR_H
