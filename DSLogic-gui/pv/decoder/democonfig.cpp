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


#include "democonfig.h"
#include "ui_i2cconfig.h"
#include "ui_spiconfig.h"
#include "ui_serialconfig.h"
#include "ui_dmx512config.h"
#include "ui_wire1config.h"

#include "decoder.h"
#include "../sigsession.h"
#include "../view/signal.h"
#include "../view/logicsignal.h"

#include <assert.h>

namespace pv {
namespace decoder {

DemoConfig::DemoConfig(QWidget *parent, struct sr_dev_inst *sdi, int protocol) :
    QDialog(parent),
    _sdi(sdi),
    _protocol(protocol)
{
    assert(_sdi);

    i2c_ui = NULL;
    spi_ui = NULL;
    wire1_ui = NULL;
    serial_ui = NULL;
    dmx512_ui = NULL;

    if (_protocol == I2C) {
        i2c_ui = new Ui::I2cConfig;
        i2c_ui->setupUi(this);

        i2c_ui->setupUi(this);
        for (const GSList *l = _sdi->probes; l; l = l->next) {
            sr_probe *const probe = (sr_probe*)l->data;
            assert(probe);
            i2c_ui->scl_comboBox->addItem(QString::number(probe->index) + " - " + probe->name);
            i2c_ui->sda_comboBox->addItem(QString::number(probe->index) + " - " + probe->name);
        }
        i2c_ui->scl_comboBox->setCurrentIndex(0);
        i2c_ui->sda_comboBox->setCurrentIndex(1);
    } else if (_protocol == SPI) {
        spi_ui = new Ui::SpiConfig;
        spi_ui->setupUi(this);

        spi_ui->setupUi(this);
        for (const GSList *l = _sdi->probes; l; l = l->next) {
            sr_probe *const probe = (sr_probe*)l->data;
            assert(probe);
            spi_ui->ssn_comboBox->addItem(QString::number(probe->index) + " - " + probe->name);
            spi_ui->sclk_comboBox->addItem(QString::number(probe->index) + " - " + probe->name);
            spi_ui->mosi_comboBox->addItem(QString::number(probe->index) + " - " + probe->name);
            spi_ui->miso_comboBox->addItem(QString::number(probe->index) + " - " + probe->name);
        }
        spi_ui->ssn_comboBox->setCurrentIndex(0);
        spi_ui->sclk_comboBox->setCurrentIndex(1);
        spi_ui->mosi_comboBox->setCurrentIndex(2);
        spi_ui->miso_comboBox->setCurrentIndex(3);
    } else if (_protocol == Serial) {
        serial_ui = new Ui::SerialConfig;
        serial_ui->setupUi(this);

        serial_ui->setupUi(this);
        for (const GSList *l = _sdi->probes; l; l = l->next) {
            sr_probe *const probe = (sr_probe*)l->data;
            assert(probe);
            serial_ui->serial_comboBox->addItem(QString::number(probe->index) + " - " + probe->name);
        }
        serial_ui->serial_comboBox->setCurrentIndex(0);
    } else if (_protocol == Dmx512) {
        dmx512_ui = new Ui::Dmx512Config;
        dmx512_ui->setupUi(this);

        dmx512_ui->setupUi(this);
        for (const GSList *l = _sdi->probes; l; l = l->next) {
            sr_probe *const probe = (sr_probe*)l->data;
            assert(probe);
            dmx512_ui->probe_comboBox->addItem(QString::number(probe->index) + " - " + probe->name);
        }
        dmx512_ui->probe_comboBox->setCurrentIndex(0);
    } else if (_protocol == Wire1) {
        wire1_ui = new Ui::Wire1Config;
        wire1_ui->setupUi(this);

        wire1_ui->setupUi(this);
        for (const GSList *l = _sdi->probes; l; l = l->next) {
            sr_probe *const probe = (sr_probe*)l->data;
            assert(probe);
            wire1_ui->probe_comboBox->addItem(QString::number(probe->index) + " - " + probe->name);
        }
        wire1_ui->probe_comboBox->setCurrentIndex(0);
    }

}

DemoConfig::~DemoConfig()
{
    if (i2c_ui)
        delete i2c_ui;
    if (spi_ui)
        delete spi_ui;
    if (serial_ui)
        delete serial_ui;
    if (dmx512_ui)
        delete dmx512_ui;
    if (wire1_ui)
        delete wire1_ui;
}

void DemoConfig::accept()
{
    using namespace Qt;

    QDialog::accept();

    if (_protocol == I2C) {
        if (!_sel_probes.empty())
            _sel_probes.clear();
        _sel_probes.push_back(i2c_ui->scl_comboBox->currentIndex());
        _sel_probes.push_back(i2c_ui->sda_comboBox->currentIndex());
    } else if (_protocol == SPI) {
        const int ssn_option = spi_ui->cs_comboBox->currentText().contains("low", Qt::CaseInsensitive) ? 0 :
                               spi_ui->cs_comboBox->currentText().contains("high", Qt::CaseInsensitive) ? 1 : -1;
        if (!_sel_probes.empty())
            _sel_probes.clear();
        if (ssn_option != -1)
            _sel_probes.push_back(spi_ui->ssn_comboBox->currentIndex());
        _sel_probes.push_back(spi_ui->sclk_comboBox->currentIndex());
        _sel_probes.push_back(spi_ui->mosi_comboBox->currentIndex());
        _sel_probes.push_back(spi_ui->miso_comboBox->currentIndex());

        if (!_options.empty())
            _options.clear();
        _options.insert("cpol", spi_ui->cpol_comboBox->currentText().contains("0", Qt::CaseInsensitive) ? 0 : 1);
        _options.insert("cpha", spi_ui->cpha_comboBox->currentText().contains("0", Qt::CaseInsensitive) ? 0 : 1);
        _options.insert("bits", spi_ui->bits_comboBox->currentText().toUInt());
        _options.insert("order", spi_ui->order_comboBox->currentText().contains("MSB", Qt::CaseInsensitive) ? 1 : 0);
        _options.insert("ssn", ssn_option);

        if (!_options_index.empty())
            _options_index.clear();
        _options_index.insert("cpol", spi_ui->cpol_comboBox->currentIndex());
        _options_index.insert("cpha", spi_ui->cpha_comboBox->currentIndex());
        _options_index.insert("bits", spi_ui->bits_comboBox->currentIndex());
        _options_index.insert("order", spi_ui->order_comboBox->currentIndex());
        _options_index.insert("ssn", spi_ui->ssn_comboBox->currentIndex());
    } else if (_protocol == Serial) {
        if (!_sel_probes.empty())
            _sel_probes.clear();
        _sel_probes.push_back(serial_ui->serial_comboBox->currentIndex());

        if (!_options.empty())
            _options.clear();
        _options.insert("baudrate", (serial_ui->baud_checkBox->checkState() == Qt::Checked) ? 0 : serial_ui->baud_comboBox->currentText().toULongLong());
        _options.insert("stopbits", serial_ui->stopbits_comboBox->currentText().toFloat());
        _options.insert("parity", serial_ui->parity_comboBox->currentText().contains("even", Qt::CaseInsensitive) ? 0 :
                                  serial_ui->parity_comboBox->currentText().contains("odd", Qt::CaseInsensitive) ? 1 : -1);
        _options.insert("order", serial_ui->order_comboBox->currentText().contains("LSB", Qt::CaseInsensitive) ? 1 : 0);
        _options.insert("bits", serial_ui->bits_comboBox->currentText().toUInt());
        _options.insert("idle", serial_ui->idle_comboBox->currentText().contains("Low", Qt::CaseInsensitive) ? 0 : 1);

        if (!_options_index.empty())
            _options_index.clear();
        _options_index.insert("baudrate", (serial_ui->baud_checkBox->checkState() == Qt::Checked) ? -1 : serial_ui->baud_comboBox->currentIndex());
        _options_index.insert("stopbits", serial_ui->stopbits_comboBox->currentIndex());
        _options_index.insert("parity", serial_ui->parity_comboBox->currentIndex());
        _options_index.insert("order", serial_ui->order_comboBox->currentIndex());
        _options_index.insert("bits", serial_ui->bits_comboBox->currentIndex());
        _options_index.insert("idle", serial_ui->idle_comboBox->currentIndex());
    } else if (_protocol == Dmx512) {
        if (!_sel_probes.empty())
            _sel_probes.clear();
        _sel_probes.push_back(dmx512_ui->probe_comboBox->currentIndex());
    } else if (_protocol == Wire1) {
        if (!_sel_probes.empty())
            _sel_probes.clear();
        _sel_probes.push_back(wire1_ui->probe_comboBox->currentIndex());
    }

}

void DemoConfig::set_config(std::list <int > sel_probes, QMap<QString, int> options_index)
{
    if (_protocol == I2C) {
        i2c_ui->scl_comboBox->setCurrentIndex(sel_probes.front());
        i2c_ui->sda_comboBox->setCurrentIndex(sel_probes.back());
    } else if (_protocol == SPI) {
        spi_ui->ssn_comboBox->setCurrentIndex(sel_probes.front());sel_probes.pop_front();
        spi_ui->sclk_comboBox->setCurrentIndex(sel_probes.front());sel_probes.pop_front();
        spi_ui->mosi_comboBox->setCurrentIndex(sel_probes.front());sel_probes.pop_front();
        spi_ui->miso_comboBox->setCurrentIndex(sel_probes.front());sel_probes.pop_front();

        spi_ui->cpol_comboBox->setCurrentIndex(options_index.value("cpol"));
        spi_ui->cpha_comboBox->setCurrentIndex(options_index.value("cpha"));
        spi_ui->bits_comboBox->setCurrentIndex(options_index.value("bits"));
        spi_ui->order_comboBox->setCurrentIndex(options_index.value("order"));
        spi_ui->ssn_comboBox->setCurrentIndex(options_index.value("ssn"));

    } else if (_protocol == Serial) {
        serial_ui->serial_comboBox->setCurrentIndex(sel_probes.front());

        if (options_index.value("baudrate") == -1)
            serial_ui->baud_checkBox->setChecked(true);
        else
            serial_ui->baud_comboBox->setCurrentIndex(options_index.value("baudrate"));
        serial_ui->stopbits_comboBox->setCurrentIndex(options_index.value("stopbits"));
        serial_ui->parity_comboBox->setCurrentIndex(options_index.value("parity"));
        serial_ui->order_comboBox->setCurrentIndex(options_index.value("order"));
        serial_ui->bits_comboBox->setCurrentIndex(options_index.value("bits"));
        serial_ui->idle_comboBox->setCurrentIndex(options_index.value("idle"));
    } else if (_protocol == Dmx512) {
        dmx512_ui->probe_comboBox->setCurrentIndex(sel_probes.front());
    } else if (_protocol == Wire1) {
        wire1_ui->probe_comboBox->setCurrentIndex(sel_probes.front());
    }

}

std::list<int> DemoConfig::get_sel_probes()
{
    return _sel_probes;
}

QMap<QString, QVariant> &DemoConfig::get_options()
{
    return _options;
}

QMap<QString, int> DemoConfig::get_options_index()
{
    return _options_index;
}

} // namespace decoder
} // namespace pv
