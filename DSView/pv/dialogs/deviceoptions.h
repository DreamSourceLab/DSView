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

#include <libsigrok.h> 
#include <QDialog>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QLabel>
#include <QCheckBox>
#include <QRadioButton>
#include <QTimer> 
#include <QWidget>
#include <vector> 
#include "../prop/binding/deviceoptions.h"
#include "../prop/binding/probeoptions.h"
#include "../toolbars/titlebar.h"
#include "../dialogs/dsdialog.h"
#include "../ui/dscombobox.h"

class QScrollArea;

class IChannelCheck
{
public:
    virtual void ChannelChecked(int index, QObject *object)=0;
};

class ChannelLabel : public QWidget 
{
Q_OBJECT

public:
    ChannelLabel(IChannelCheck *check, QWidget *parent, int chanIndex);

    inline QCheckBox* getCheckBox(){
        return _box;
    }

private slots:
    void on_checked();

private:
    QCheckBox *_box;
    IChannelCheck *_checked;
    int     _index;
};

struct ChannelModePair
{
    void *key;
    int  value;
};

class DeviceAgent;

namespace pv {

namespace dialogs {

class DeviceOptions : public DSDialog, public IChannelCheck
{
	Q_OBJECT

public:
    DeviceOptions(QWidget *parent);

    ~DeviceOptions();

protected:
	void accept();
    void reject();

private:
    QLayout *get_property_form(QWidget *parent);

    void logic_probes(QVBoxLayout& layout);
    void analog_probes(QGridLayout& layout);
    QString dynamic_widget(QLayout *lay); 

	void set_all_probes(bool set);
    void enable_max_probes(); 
    void build_dynamic_panel();
    void try_resize_scroll();

    void channel_checkbox_clicked(QCheckBox *sc);

private:
    //IChannelCheck
    void ChannelChecked(int index, QObject *object);

private slots:
	void enable_all_probes();
	void disable_all_probes();
    void zero_adj();
    void mode_check_timeout();
    void channel_check();
    void analog_channel_check();
    void on_calibration();
    void on_analog_channel_enable();
    void on_anlog_tab_changed(int index);
    
private: 
    std::vector<QCheckBox *> _probes_checkBox_list;

    QTimer      _mode_check_timer;
    int         _opt_mode;  
    QWidget     *_scroll_panel;
    QScrollArea *_scroll;
    QWidget     *_container_panel;
    QVBoxLayout *_container_lay;   
    QWidget     *_dynamic_panel;
   // QWidget     *_dynamic_panel;
    int     _width;
    int     _groupHeight1;
    int     _groupHeight2;
    volatile    bool _isBuilding;
    DeviceAgent *_device_agent;
    int     _cur_analog_tag_index;
    QString _demo_operation_mode;

	pv::prop::binding::DeviceOptions _device_options_binding;
    std::vector<pv::prop::binding::ProbeOptions *> _probe_options_binding_list;
    std::vector<ChannelModePair> _channel_mode_indexs;
    std::vector<struct sr_channel*>     _dso_channel_list;
};

} // namespace dialogs
} // namespace pv

#endif // DSVIEW_PV_DEVICEOPTIONS_H
