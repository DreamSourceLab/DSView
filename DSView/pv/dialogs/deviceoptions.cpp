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

#include "deviceoptions.h"
 
#include <QListWidget>
#include <QSpinBox>
#include <QDoubleSpinBox> 
#include <QGuiApplication>
#include <QScreen>
#include <QScrollArea>
#include <QLayoutItem>
#include <assert.h>

#include "dsmessagebox.h"
#include "../prop/property.h"
#include "../dsvdef.h"
#include "../config/appconfig.h"
#include "../appcontrol.h"
#include "../sigsession.h"

using namespace boost;
using namespace std;

ChannelLabel::ChannelLabel(IChannelCheck *check, QWidget *parent, int chanIndex)
: QWidget(parent)
{
    _checked = check;
    _index = chanIndex;  

    this->setFixedSize(30, 40);
   
    QVBoxLayout *lay = new QVBoxLayout();
    lay->setContentsMargins(0,0,0,0);
    lay->setSpacing(0);
    this->setLayout(lay);
    QHBoxLayout *h1 = new QHBoxLayout();
    QHBoxLayout *h2 = new QHBoxLayout();
    h2->setAlignment(Qt::AlignCenter);
    lay->addLayout(h1);
    lay->addLayout(h2);
    QLabel *lab = new QLabel(QString::number(chanIndex));
    lab->setAlignment(Qt::AlignCenter);
    h1->addWidget(lab);
    _box = new QCheckBox();
    h2->addWidget(_box);

    connect(_box, SIGNAL(released()), this, SLOT(on_checked()));
}

void ChannelLabel::on_checked()
{
    assert(_checked);
    _checked->ChannelChecked(_index);
}

//--------------------------
 
namespace pv {
namespace dialogs {

DeviceOptions::DeviceOptions(QWidget *parent) :
    DSDialog(parent)
{
    _scroll_panel = NULL;
    _container_panel = NULL;   
    _scroll = NULL; 
    _width = 0;
    _groupHeight1 = 0;
    _groupHeight2 = 0;
    _dynamic_panel = NULL;
    _container_lay = NULL;
    _isBuilding = false;

    SigSession *session = AppControl::Instance()->GetSession();
    _device_agent = session->get_device();

    this->setTitle(tr("Device Options"));
    this->SetTitleSpace(0);
    this->layout()->setSpacing(0);
    this->layout()->setDirection(QBoxLayout::TopToBottom);
    this->layout()->setAlignment(Qt::AlignTop); 

    // scroll panel
    _scroll_panel  = new QWidget();
    QVBoxLayout *scroll_lay = new QVBoxLayout();
    scroll_lay->setContentsMargins(0, 0, 0, 0);
    scroll_lay->setAlignment(Qt::AlignLeft);
    scroll_lay->setDirection(QBoxLayout::TopToBottom);
    _scroll_panel->setLayout(scroll_lay);
    this->layout()->addWidget(_scroll_panel);

    // container
    _container_panel = new QWidget();      
    _container_lay = new QVBoxLayout();
    _container_lay->setDirection(QBoxLayout::TopToBottom);
    _container_lay->setAlignment(Qt::AlignTop);
    _container_lay->setContentsMargins(0, 0, 0, 0);
    _container_lay->setSpacing(5);
    _container_panel->setLayout(_container_lay);
    scroll_lay->addWidget(_container_panel);
    //_container_panel->setStyleSheet("background-color:red");
    
    // mode group box
    QGroupBox *props_box = new QGroupBox(tr("Mode"), this);
    QLayout *props_lay = get_property_form(props_box);
    props_box->setLayout(props_lay);
    _container_lay->addWidget(props_box);

    // chnnels group box
    this->build_dynamic_panel();

    // space
    QWidget *space = new QWidget();
    space->setMinimumHeight(5);
    this->layout()->addWidget(space);

    //button
    auto button_box = new QDialogButtonBox(QDialogButtonBox::Ok, Qt::Horizontal, this);
	this->layout()->addWidget(button_box); 
   
    GVariant* gvar = _device_agent->get_config(NULL, NULL, SR_CONF_OPERATION_MODE);
    if (gvar != NULL) {
        _mode = QString::fromUtf8(g_variant_get_string(gvar, NULL));
        g_variant_unref(gvar);
    }

    try_resize_scroll();
  
    connect(&_mode_check, SIGNAL(timeout()), this, SLOT(mode_check()));
    connect(button_box, SIGNAL(accepted()), this, SLOT(accept()));

    _mode_check.setInterval(100);
    _mode_check.start();  
}

DeviceOptions::~DeviceOptions(){   
} 

void DeviceOptions::accept()
{
	using namespace Qt;
    bool hasEnabled = false;

	// Commit the properties
    const auto &dev_props = _device_options_binding.properties();
    for(auto &p : dev_props) {
		assert(p);
		p->commit();
	}

    // Commit the probes
    int mode = _device_agent->get_work_mode();
    if (mode == LOGIC || mode == ANALOG) {
        int index = 0;
        for (const GSList *l = _device_agent->get_channels(); l; l = l->next) {
            sr_channel *const probe = (sr_channel*)l->data;
            assert(probe);
            probe->enabled = _probes_checkBox_list.at(index)->isChecked();
            index++;
            if (probe->enabled)
                hasEnabled = true;
        }
    } else {
        hasEnabled = true;
    }

    if (hasEnabled) {
        QVector<pv::prop::binding::ProbeOptions *>::iterator i = _probe_options_binding_list.begin();
        while(i != _probe_options_binding_list.end()) {
            const auto &probe_props = (*i)->properties();

            for(auto &p :probe_props) {
                assert(p);
                p->commit();
            }
            i++;
        }

        QDialog::accept();
    } else {
        dialogs::DSMessageBox msg(this);
        msg.mBox()->setText(tr("Attention"));
        msg.mBox()->setInformativeText(tr("All channel disabled! Please enable at least one channel."));
        msg.mBox()->addButton(tr("Ok"), QMessageBox::AcceptRole);
        msg.mBox()->setIcon(QMessageBox::Warning);
        msg.exec();
    }
}

void DeviceOptions::reject()
{
    using namespace Qt;

    QDialog::reject();
}

QLayout * DeviceOptions::get_property_form(QWidget * parent)
{
    QGridLayout *const layout = new QGridLayout(parent);
    layout->setVerticalSpacing(2);
    const auto &properties =_device_options_binding.properties();

    int i = 0;
    for(auto &p : properties)
	{
		assert(p);
        const QString label = p->labeled_widget() ? QString() : p->label();
        layout->addWidget(new QLabel(label, parent), i, 0);
        if (label == tr("Operation Mode"))
            layout->addWidget(p->get_widget(parent, true), i, 1);
        else
            layout->addWidget(p->get_widget(parent), i, 1);
        layout->setRowMinimumHeight(i, 22);
        i++;
	}

    //_groupHeight1 = i * 22 + 180;
    _groupHeight1 = parent->sizeHint().height(); 
    parent->setFixedHeight(_groupHeight1); 

    return layout;
}

void DeviceOptions::logic_probes(QVBoxLayout &layout)
{
	using namespace Qt;

    layout.setSpacing(2);

    int row1 = 0;
    int row2 = 0;
    int index = 0;
    int vld_ch_num = 0;
    int cur_ch_num = 0;
    int contentHeight = 0;
 
    _probes_checkBox_list.clear();
  
    //channel count checked
    if (_device_agent->get_work_mode()== LOGIC) {
        GVariant *gvar_opts;
        gsize num_opts;

        gvar_opts = _device_agent->get_config_list(NULL, SR_CONF_CHANNEL_MODE);

        if (gvar_opts != NULL) 
        {
            const char **const options = g_variant_get_strv(gvar_opts, &num_opts);
            GVariant* gvar = _device_agent->get_config(NULL, NULL, SR_CONF_CHANNEL_MODE);
            if (gvar != NULL) {
                QString ch_mode = QString::fromUtf8(g_variant_get_string(gvar, NULL));
                g_variant_unref(gvar);

                for (unsigned int i=0; i<num_opts; i++){
                    QRadioButton *ch_opts = new QRadioButton(options[i]);
                    
                    layout.addWidget(ch_opts); 
                    contentHeight += ch_opts->sizeHint().height();  //radio button height
                   
                    connect(ch_opts, SIGNAL(pressed()), this, SLOT(channel_check()));

                    row1++;
                    if (QString::fromUtf8(options[i]) == ch_mode)
                        ch_opts->setChecked(true); 
                }
            }
            if (gvar_opts)
                g_variant_unref(gvar_opts);
        }
    }

    GVariant *gvar =  _device_agent->get_config(NULL, NULL, SR_CONF_VLD_CH_NUM);
    if (gvar != NULL) {
        vld_ch_num = g_variant_get_int16(gvar);
        g_variant_unref(gvar);
    }

    // channels
    QHBoxLayout *line_lay = NULL;

    for (const GSList *l = _device_agent->get_channels(); l; l = l->next) {
		sr_channel *const probe = (sr_channel*)l->data;
		assert(probe);

        if (probe->enabled)
            cur_ch_num++;

        if (cur_ch_num > vld_ch_num)
            probe->enabled = false;

        if (line_lay == NULL){
            line_lay = new QHBoxLayout();
            layout.addLayout(line_lay);
            _sub_lays.push_back(line_lay);
            row2++;                
        }

        ChannelLabel *lab = new ChannelLabel(this, NULL, probe->index);
        line_lay->addWidget(lab);      
        _probes_checkBox_list.push_back(lab->getCheckBox());
        lab->getCheckBox()->setCheckState(probe->enabled ? Qt::Checked : Qt::Unchecked); 

        index++;

        if (index % 8 == 0){
            line_lay = NULL;
        } 
	}

    // space
    QWidget *space = new QWidget();
    space->setFixedHeight(10);
    layout.addWidget(space);
    contentHeight += 10;
 
    // buttons
     line_lay = new QHBoxLayout();
    layout.addLayout(line_lay);
    _sub_lays.push_back(line_lay);
    line_lay->setSpacing(10);

    QPushButton *enable_all_probes = new QPushButton(tr("Enable All"));
    QPushButton *disable_all_probes = new QPushButton(tr("Disable All"));
    enable_all_probes->setMaximumHeight(30);
    disable_all_probes->setMaximumHeight(30);

    contentHeight += enable_all_probes->sizeHint().height();
    contentHeight += row2 * 40;

    connect(enable_all_probes, SIGNAL(clicked()),
            this, SLOT(enable_all_probes()));
    connect(disable_all_probes, SIGNAL(clicked()),
            this, SLOT(disable_all_probes()));

    line_lay->addWidget(enable_all_probes);
    line_lay->addWidget(disable_all_probes);

    _groupHeight2 = contentHeight + (row1 + row2) * 2 + 38;

#ifdef Q_OS_DARWIN
    _groupHeight2 += 5;
#endif

    _dynamic_panel->setFixedHeight(_groupHeight2); 
}

void DeviceOptions::set_all_probes(bool set)
{ 
    for (auto box : _probes_checkBox_list) {
        box->setCheckState(set ? Qt::Checked : Qt::Unchecked);
    }
}

void DeviceOptions::enable_max_probes() {
    int cur_ch_num = 0; 
    for (auto box : _probes_checkBox_list) {
        if (box->isChecked())
            cur_ch_num++;
    }

    GVariant* gvar =  _device_agent->get_config(NULL, NULL, SR_CONF_VLD_CH_NUM);
    if (gvar == NULL)
        return;

    int vld_ch_num = g_variant_get_int16(gvar);
    g_variant_unref(gvar); 

    while(cur_ch_num < vld_ch_num  && cur_ch_num < (int)_probes_checkBox_list.size()) {
        auto box = _probes_checkBox_list[cur_ch_num];
        if (box->isChecked() == false) {
            box->setChecked(true);
            cur_ch_num++;
        } 
    }
}

void DeviceOptions::enable_all_probes()
{
    GVariant* gvar = _device_agent->get_config(NULL, NULL, SR_CONF_STREAM);
    if (gvar != NULL) {
        bool stream_mode = g_variant_get_boolean(gvar);
        g_variant_unref(gvar);

        if (stream_mode) {
            enable_max_probes();
            return;
        }
    }

	set_all_probes(true);
}

void DeviceOptions::disable_all_probes()
{
	set_all_probes(false);
}

void DeviceOptions::zero_adj()
{
    using namespace Qt;
    QDialog::accept();

    dialogs::DSMessageBox msg(this);
    msg.mBox()->setText(tr("Information"));
    msg.mBox()->setInformativeText(tr("Auto Calibration program will be started. Don't connect any probes. It can take a while!"));
    msg.mBox()->addButton(tr("Ok"), QMessageBox::AcceptRole);
    msg.mBox()->addButton(tr("Cancel"), QMessageBox::RejectRole);
    msg.mBox()->setIcon(QMessageBox::Information);

    if (msg.exec()) {
        _device_agent->set_config(NULL, NULL, SR_CONF_ZERO, g_variant_new_boolean(true));
    } else {
        _device_agent->set_config(NULL, NULL, SR_CONF_ZERO, g_variant_new_boolean(false));
    }
}

void DeviceOptions::on_calibration()
{
    using namespace Qt;
    QDialog::accept();
    _device_agent->set_config(NULL, NULL, SR_CONF_CALI, g_variant_new_boolean(true));
}

void DeviceOptions::mode_check()
{
    if (_isBuilding)
        return;

    bool test;
    QString mode;
    GVariant* gvar = _device_agent->get_config(NULL, NULL, SR_CONF_OPERATION_MODE);
    if (gvar != NULL) {
        mode = QString::fromUtf8(g_variant_get_string(gvar, NULL));
        g_variant_unref(gvar);

        if (mode != _mode) { 
            _mode = mode; 
            build_dynamic_panel();
            try_resize_scroll();
        }
    }

    gvar = _device_agent->get_config(NULL, NULL, SR_CONF_TEST);
    if (gvar != NULL) {
        test = g_variant_get_boolean(gvar);
        g_variant_unref(gvar);

        if (test) { 
            for (auto box : _probes_checkBox_list) {
                box->setCheckState(Qt::Checked);
                box->setDisabled(true);
            }
        }
    } 
}

void DeviceOptions::channel_check()
{
    QRadioButton* sc=dynamic_cast<QRadioButton*>(sender());
    QString text = sc->text();
    text.remove('&');

    if(sc != NULL){
        _device_agent->set_config(NULL, NULL, SR_CONF_CHANNEL_MODE, g_variant_new_string(text.toUtf8().data()));
    }
    
    build_dynamic_panel();
    try_resize_scroll();
}

void DeviceOptions::analog_channel_check()
{
    QCheckBox* sc=dynamic_cast<QCheckBox*>(sender());
    if(sc != NULL) {
        for (const GSList *l = _device_agent->get_channels(); l; l = l->next) {
            sr_channel *const probe = (sr_channel*)l->data;
            assert(probe);
            if (sc->property("index").toInt() == probe->index)
               _device_agent->set_config(probe, NULL, SR_CONF_PROBE_MAP_DEFAULT,
                                     g_variant_new_boolean(sc->isChecked()));
        }
    }

    build_dynamic_panel();
    try_resize_scroll();
}

void DeviceOptions::channel_enable()
{
    if (_device_agent->get_work_mode() == LOGIC) {
        QCheckBox* sc=dynamic_cast<QCheckBox*>(sender());
        if (sc == NULL || !sc->isChecked())
            return;

        GVariant* gvar = _device_agent->get_config(NULL, NULL, SR_CONF_STREAM);
        if (gvar == NULL)
            return;

        bool stream_mode = g_variant_get_boolean(gvar);
        g_variant_unref(gvar);

        if (!stream_mode)
            return;

        int cur_ch_num = 0; 
        for (auto box : _probes_checkBox_list) {
            if (box->isChecked())
                cur_ch_num++;
        }

        gvar =  _device_agent->get_config(NULL, NULL, SR_CONF_VLD_CH_NUM);
        if (gvar == NULL)
            return;

        int vld_ch_num = g_variant_get_int16(gvar);
        g_variant_unref(gvar);
        if (cur_ch_num > vld_ch_num) {
            dialogs::DSMessageBox msg(this);
            msg.mBox()->setText(tr("Information"));
            msg.mBox()->setInformativeText(tr("Current mode only suppport max ") + QString::number(vld_ch_num) + tr(" channels!"));
            msg.mBox()->addButton(tr("Ok"), QMessageBox::AcceptRole);
            msg.mBox()->setIcon(QMessageBox::Information);
            msg.exec();

            sc->setChecked(false);
        }
    }
    else if (_device_agent->get_work_mode() == ANALOG) {
        QCheckBox* sc=dynamic_cast<QCheckBox*>(sender());
        if (sc != NULL) {
            QGridLayout *const layout = (QGridLayout *)sc->property("Layout").value<void *>();
            int i = layout->count();
            while(i--)
            {
                QWidget* w = layout->itemAt(i)->widget();
                if (w->property("Enable").isNull()) {
                    w->setEnabled(sc->isChecked());
                }
            }
        }
    }
} 

void DeviceOptions::analog_probes(QGridLayout &layout)
{
    using namespace Qt;
 
    _probes_checkBox_list.clear();
    _probe_options_binding_list.clear();

    QTabWidget *tabWidget = new QTabWidget();
    tabWidget->setTabPosition(QTabWidget::North);
    tabWidget->setUsesScrollButtons(false);
    //layout.setContentsMargins(0,20, 0, 10);

    for (const GSList *l = _device_agent->get_channels(); l; l = l->next) {
        sr_channel *const probe = (sr_channel*)l->data;
        assert(probe);

        QWidget *probe_widget = new QWidget(tabWidget);
        QGridLayout *probe_layout = new QGridLayout(probe_widget);
        probe_widget->setLayout(probe_layout); 

        QCheckBox *probe_checkBox = new QCheckBox(this);
        QVariant vlayout = QVariant::fromValue((void *)probe_layout);
        probe_checkBox->setProperty("Layout", vlayout);
        probe_checkBox->setProperty("Enable", true);
        probe_checkBox->setCheckState(probe->enabled ? Qt::Checked : Qt::Unchecked);
        _probes_checkBox_list.push_back(probe_checkBox);

        QLabel *en_label = new QLabel(tr("Enable: "), this);
        en_label->setProperty("Enable", true);
        probe_layout->addWidget(en_label, 0, 0, 1, 1);
        probe_layout->addWidget(probe_checkBox, 0, 1, 1, 3);

        pv::prop::binding::ProbeOptions *probe_options_binding =
                new pv::prop::binding::ProbeOptions(probe);
        const auto &properties = probe_options_binding->properties();
        int i = 1;
        
        for(auto &p : properties)
        {
            assert(p);
            const QString label = p->labeled_widget() ? QString() : p->label();
            probe_layout->addWidget(new QLabel(label, probe_widget), i, 0, 1, 1);

            QWidget * pow = p->get_widget(probe_widget);
            pow->setEnabled(probe_checkBox->isChecked());
            if (p->name().contains("Map Default")) {
                pow->setProperty("index", probe->index);
                connect(pow, SIGNAL(clicked()), this, SLOT(analog_channel_check()));
            } else {
                if (probe_checkBox->isChecked() && p->name().contains("Map")) {
                    bool map_default = true;
                    GVariant* gvar = _device_agent->get_config(probe, NULL, SR_CONF_PROBE_MAP_DEFAULT);
                    if (gvar != NULL) {
                        map_default =g_variant_get_boolean(gvar);
                        g_variant_unref(gvar);
                    }
                    if (map_default)
                        pow->setEnabled(false);
                }
            }
            probe_layout->addWidget(pow, i, 1, 1, 3);
            i++;
        }
        _probe_options_binding_list.push_back(probe_options_binding);

        connect(probe_checkBox, SIGNAL(released()), this, SLOT(channel_enable()));

        tabWidget->addTab(probe_widget, QString::fromUtf8(probe->name));
    }

    layout.addWidget(tabWidget, 0, 0, 1, 1);

    _groupHeight2 = tabWidget->sizeHint().height() + 10;
    _dynamic_panel->setFixedHeight(_groupHeight2); 
}

void DeviceOptions::ChannelChecked(int index)
{
   channel_enable();
}

QString DeviceOptions::dynamic_widget(QLayout *lay)
 { 
    int mode = _device_agent->get_work_mode();

    if (mode == LOGIC) {
        QVBoxLayout *grid = dynamic_cast<QVBoxLayout*>(lay);
        assert(grid);
        logic_probes(*grid);
        return tr("Channels");
    } 
    else if (mode == DSO) {
        GVariant* gvar = _device_agent->get_config(NULL, NULL, SR_CONF_HAVE_ZERO);
        if (gvar != NULL) {
            bool have_zero = g_variant_get_boolean(gvar);
            g_variant_unref(gvar);

            QGridLayout *grid = dynamic_cast<QGridLayout*>(lay);
            assert(grid);

            if (have_zero) {
                auto config_button = new QPushButton(tr("Auto Calibration"), this);
                grid->addWidget(config_button, 0, 0, 1, 1);
                connect(config_button, SIGNAL(clicked()), this, SLOT(zero_adj()));

                auto cali_button = new QPushButton(tr("Manual Calibration"), this);
                grid->addWidget(cali_button, 1, 0, 1, 1);
                connect(cali_button, SIGNAL(clicked()), this, SLOT(on_calibration()));

                _groupHeight2 = 100;
                _dynamic_panel->setFixedHeight(_groupHeight2); 

                return tr("Calibration");
            }
        }
    } 
    else if (mode == ANALOG) {
        QGridLayout *grid = dynamic_cast<QGridLayout*>(lay);
        assert(grid);
        analog_probes(*grid);
        return tr("Channels");
    }
    return NULL;
}

void DeviceOptions::build_dynamic_panel()
{
    _isBuilding = true;

    if (_dynamic_panel != NULL){
        _dynamic_panel->deleteLater();
        _dynamic_panel = NULL;
    }

    if (_dynamic_panel == NULL)
    {  
        _dynamic_panel = new QGroupBox("group", _dynamic_panel);
        _container_lay->addWidget(_dynamic_panel);

        if (_device_agent->get_work_mode() == LOGIC)
            _dynamic_panel->setLayout(new QVBoxLayout());
        else
            _dynamic_panel->setLayout(new QGridLayout());
    }
 
    QString title = dynamic_widget(_dynamic_panel->layout());
    QGroupBox *box = dynamic_cast<QGroupBox*>(_dynamic_panel);    
    box->setTitle(title);

    if (title == ""){ 
        box->setVisible(false);
    }

    _isBuilding = false;
}

int bb = 0;
void DeviceOptions::try_resize_scroll()
{
    // content area height
    int contentHeight = _groupHeight1 + _groupHeight2 + 10; // +space
    //dialog height
    int dlgHeight = contentHeight + 100; // +bottom buttton

#ifdef Q_OS_DARWIN
    dlgHeight += 20;
#endif

    float sk = QGuiApplication::primaryScreen()->logicalDotsPerInch() / 96;

    int srcHeight = 600;
    int w = _width;

    if (w == 0)
    {
        w = this->sizeHint().width();
        _width = w;
    }

    QScrollArea *scroll = _scroll;
    if (scroll == NULL)
    {
        scroll = new QScrollArea(_scroll_panel);
        scroll->setWidget(_container_panel);
        scroll->setStyleSheet("QScrollArea{border:none;}");
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        _scroll = scroll;
    }

    _container_panel->setFixedHeight(contentHeight);
    int sclw = w - 23;

#ifdef Q_OS_DARWIN
    sclw -= 20;
#endif

    if (sk * dlgHeight > srcHeight)
    {
        int exth = 120;
        this->setFixedSize(w + 12, srcHeight);
        _scroll_panel->setFixedSize(w, srcHeight - exth);
        _scroll->setFixedSize(sclw, srcHeight - exth);
    }
    else
    { 
        this->setFixedSize(w + 12, dlgHeight);
        _scroll_panel->setFixedSize(w, contentHeight);
        _scroll->setFixedSize(sclw, contentHeight);
    }
}

} // namespace dialogs
} // namespace pv
