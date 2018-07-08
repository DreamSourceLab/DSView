/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
 * Copyright (C) 2012 Joel Holdsworth <joel@airwebreathe.org.uk>
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


#ifndef DSVIEW_PV_DEVICEOPTIONS_H
#define DSVIEW_PV_DEVICEOPTIONS_H

#include <QDialog>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QVector>
#include <QLabel>
#include <QCheckBox>
#include <QComboBox>
#include <QRadioButton>
#include <QTimer>

#include <boost/shared_ptr.hpp>

#include <libsigrok4DSL/libsigrok.h>
#include "../device/devinst.h"
#include "../prop/binding/deviceoptions.h"
#include "../prop/binding/probeoptions.h"
#include "../toolbars/titlebar.h"
#include "../dialogs/dsdialog.h"

namespace pv {
namespace dialogs {

class DeviceOptions : public DSDialog
{
	Q_OBJECT

public:
    DeviceOptions(QWidget *parent, boost::shared_ptr<pv::device::DevInst> dev_inst);

protected:
	void accept();
    void reject();

private:

    QGridLayout *get_property_form(QWidget *parent);

    void logic_probes(QGridLayout& layout);
    void analog_probes(QGridLayout& layout);
    QString dynamic_widget(QGridLayout &_dynamic_layout);

	void set_all_probes(bool set);
    void enable_max_probes();

private slots:
	void enable_all_probes();
	void disable_all_probes();
    void zero_adj();
    void mode_check();
    void channel_check();
    void on_calibration();
    void channel_enable();

private:
    boost::shared_ptr<pv::device::DevInst>  _dev_inst;
	QVBoxLayout _layout;
    toolbars::TitleBar *_titlebar;

    QGroupBox *_dynamic_box;
    QGridLayout _dynamic_layout;
    QVector <QLabel *> _probes_label_list;
    QVector <QCheckBox *> _probes_checkBox_list;
    QVector <QWidget *> _probe_widget_list;

    QGroupBox *_props_box;

    QPushButton *_config_button;
    QPushButton *_cali_button;
	QDialogButtonBox _button_box;

    QTimer _mode_check;
    QString _mode;

	pv::prop::binding::DeviceOptions _device_options_binding;
    QVector <pv::prop::binding::ProbeOptions *> _probe_options_binding_list;
};

} // namespace dialogs
} // namespace pv

#endif // DSVIEW_PV_DEVICEOPTIONS_H
