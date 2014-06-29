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

#include <QComboBox>
#include <QToolBar>
#include <QToolButton>

#include <libsigrok4DSLogic/libsigrok.h>

struct st_dev_inst;
class QAction;

namespace pv {
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
	SamplingBar(QWidget *parent);

	uint64_t get_record_length() const;
    void set_record_length(uint64_t length);

	void set_sampling(bool sampling);
    void update_sample_rate_selector();
    void set_sample_rate(uint64_t sample_rate);

    void set_device(struct sr_dev_inst *sdi);

    void enable_toggle(bool enable);

    void enable_run_stop(bool enable);

signals:
	void run_stop();
    void device_reload();

private:
	void update_sample_rate_selector_value();
	void commit_sample_rate();

private slots:
	void on_sample_rate_changed();
	void on_run_stop();

private:
    bool _enable;

    struct sr_dev_inst *_sdi;

	QComboBox _record_length_selector;

	QComboBox _sample_rate_list;
	QAction *_sample_rate_list_action;

    QIcon _icon_stop;
    QIcon _icon_start;
	QToolButton _run_stop_button;
};

} // namespace toolbars
} // namespace pv

#endif // DSLOGIC_PV_TOOLBARS_SAMPLINGBAR_H
