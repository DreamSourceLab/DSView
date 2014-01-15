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


#ifndef DSLOGIC_PV_DEMOCONFIG_H
#define DSLOGIC_PV_DEMOCONFIG_H

#include <QDialog>
#include <QMap>
#include <QVariant>

#include "../sigsession.h"

#include <libsigrok4DSLogic/libsigrok.h>

#include <vector>

namespace Ui {
class I2cConfig;
class SpiConfig;
class SerialConfig;
class Dmx512Config;
class Wire1Config;
}

namespace pv {
namespace decoder {

class DemoConfig : public QDialog
{
    Q_OBJECT

public:

    DemoConfig(QWidget *parent = 0, sr_dev_inst *sdi = 0, int protocol = 0);
    virtual ~DemoConfig();

    void set_config(std::list <int > sel_probes, QMap<QString, int> options_index);

    std::list<int> get_sel_probes();

    QMap <QString, QVariant>& get_options();

    QMap <QString, int> get_options_index();

protected:
    void accept();

signals:
    
public slots:
    
private:
    sr_dev_inst *_sdi;

    int _protocol;

    std::list <int > _sel_probes;
    QMap <QString, QVariant> _options;
    QMap <QString, int> _options_index;

    Ui::I2cConfig *i2c_ui;
    Ui::SpiConfig *spi_ui;
    Ui::SerialConfig *serial_ui;
    Ui::Dmx512Config *dmx512_ui;
    Ui::Wire1Config *wire1_ui;
};

} // namespace decoder
} // namespace pv

#endif // DSLOGIC_PV_DEMOCONFIG_H
