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


#include <extdef.h>

#include <assert.h>

#include <boost/foreach.hpp>

#include <libsigrok4DSLogic/libsigrok.h>

#include <QAction>
#include <QDebug>
#include <QLabel>

#include "samplingbar.h"

using namespace std;

namespace pv {
namespace toolbars {

const uint64_t SamplingBar::RecordLengths[19] = {
	1000,
    2000,
    4000,
    8000,
	10000,
    20000,
    40000,
    80000,
	100000,
    200000,
    400000,
    800000,
	1000000,
	2000000,
    4000000,
    8000000,
	10000000,
    20000000,
    40000000,
};

const uint64_t SamplingBar::DefaultRecordLength = 1000000;

const uint64_t SamplingBar::DSLogic_RecordLengths[15] = {
    1024,
    2048,
    4096,
    8192,
    16384,
    32768,
    65536,
    131072,
    262144,
    524288,
    1048576,
    2097152,
    4194304,
    8388608,
    16777216,
};

const uint64_t SamplingBar::DSLogic_DefaultRecordLength = 16777216;

SamplingBar::SamplingBar(QWidget *parent) :
	QToolBar("Sampling Bar", parent),
	_record_length_selector(this),
	_sample_rate_list(this),
    _icon_stop(":/icons/stop.png"),
    _icon_start(":/icons/start.png"),
	_run_stop_button(this)
{
    setMovable(false);

	connect(&_run_stop_button, SIGNAL(clicked()),
		this, SLOT(on_run_stop()));

//	for (size_t i = 0; i < countof(RecordLengths); i++)
//	{
//		const uint64_t &l = RecordLengths[i];
//		char *const text = ds_si_string_u64(l, " samples");
//		_record_length_selector.addItem(QString(text),
//			qVariantFromValue(l));
//		g_free(text);

//		if (l == DefaultRecordLength)
//			_record_length_selector.setCurrentIndex(i);
//	}
    _record_length_selector.setSizeAdjustPolicy(QComboBox::AdjustToContents);
	set_sampling(false);

    //_run_stop_button.setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    _run_stop_button.setObjectName(tr("run_stop_button"));

    addWidget(new QLabel(tr(" ")));
    addWidget(&_record_length_selector);
    addWidget(new QLabel(tr(" @ ")));
	_sample_rate_list_action = addWidget(&_sample_rate_list);
	addWidget(&_run_stop_button);

	connect(&_sample_rate_list, SIGNAL(currentIndexChanged(int)),
		this, SLOT(on_sample_rate_changed()));
}

void SamplingBar::set_device(struct sr_dev_inst *sdi)
{
    assert(sdi);
    _sdi = sdi;
    if (strcmp(sdi->driver->name, "DSLogic") == 0) {
        _record_length_selector.clear();
        for (size_t i = 0; i < countof(DSLogic_RecordLengths); i++)
        {
            const uint64_t &l = DSLogic_RecordLengths[i];
            char *const text = sr_iec_string_u64(l, " samples");
            _record_length_selector.addItem(QString(text),
                qVariantFromValue(l));
            g_free(text);

            if (l == DSLogic_DefaultRecordLength)
                _record_length_selector.setCurrentIndex(i);
        }
    } else {
        for (size_t i = 0; i < countof(RecordLengths); i++)
        {
            const uint64_t &l = RecordLengths[i];
            char *const text = sr_si_string_u64(l, " samples");
            _record_length_selector.addItem(QString(text),
                qVariantFromValue(l));
            g_free(text);

            if (l == DefaultRecordLength)
                _record_length_selector.setCurrentIndex(i);
        }
    }
}

uint64_t SamplingBar::get_record_length() const
{
	const int index = _record_length_selector.currentIndex();
	if (index < 0)
		return 0;

	return _record_length_selector.itemData(index).value<uint64_t>();
}

void SamplingBar::set_record_length(uint64_t length)
{
    for (int i = 0; i < _record_length_selector.count(); i++) {
        if (length == _record_length_selector.itemData(
            i).value<uint64_t>()) {
            _record_length_selector.setCurrentIndex(i);
            break;
        }
    }
}

void SamplingBar::set_sampling(bool sampling)
{
    _run_stop_button.setIcon(sampling ? _icon_stop : _icon_start);
    //_run_stop_button.setText(sampling ? " Stop" : "Start");
}

void SamplingBar::set_sample_rate(uint64_t sample_rate)
{
    for (int i = 0; i < _sample_rate_list.count(); i++) {
        if (sample_rate == _sample_rate_list.itemData(
            i).value<uint64_t>()) {
            _sample_rate_list.setCurrentIndex(i);
            // Set the samplerate
            if (sr_config_set(_sdi, SR_CONF_SAMPLERATE,
                g_variant_new_uint64(sample_rate)) != SR_OK) {
                qDebug() << "Failed to configure samplerate.";
                return;
            }
            break;
        }
    }

}

void SamplingBar::update_sample_rate_selector()
{
	GVariant *gvar_dict, *gvar_list;
	const uint64_t *elements = NULL;
	gsize num_elements;
	//QAction *selector_action = NULL;

	assert(_sample_rate_list_action);

    if (!_sdi)
		return;

    if (sr_config_list(_sdi->driver, SR_CONF_SAMPLERATE,
            &gvar_dict, _sdi) != SR_OK)
		return;

	_sample_rate_list_action->setVisible(false);

    if ((gvar_list = g_variant_lookup_value(gvar_dict,
			"samplerates", G_VARIANT_TYPE("at"))))
	{
		elements = (const uint64_t *)g_variant_get_fixed_array(
				gvar_list, &num_elements, sizeof(uint64_t));
		_sample_rate_list.clear();

		for (unsigned int i = 0; i < num_elements; i++)
		{
			char *const s = sr_samplerate_string(elements[i]);
			_sample_rate_list.addItem(QString(s),
				qVariantFromValue(elements[i]));
			g_free(s);
		}

		_sample_rate_list.show();
		g_variant_unref(gvar_list);

		//selector_action = _sample_rate_list_action;
	}

	g_variant_unref(gvar_dict);
    _sample_rate_list_action->setVisible(true);
    update_sample_rate_selector_value();
}

void SamplingBar::update_sample_rate_selector_value()
{
	GVariant *gvar;
	uint64_t samplerate;

    assert(_sdi);

    if (sr_config_get(_sdi->driver, SR_CONF_SAMPLERATE,
        &gvar, _sdi) != SR_OK) {
		qDebug() <<
				"WARNING: Failed to get value of sample rate";
		return;
	}
	samplerate = g_variant_get_uint64(gvar);
	g_variant_unref(gvar);

	assert(_sample_rate_list_action);

    if (_sample_rate_list_action->isVisible())
	{
		for (int i = 0; i < _sample_rate_list.count(); i++)
			if (samplerate == _sample_rate_list.itemData(
				i).value<uint64_t>())
				_sample_rate_list.setCurrentIndex(i);
	}
}

void SamplingBar::commit_sample_rate()
{
    GVariant *gvar;
	uint64_t sample_rate = 0;
    uint64_t last_sample_rate = 0;

    assert(_sdi);

	assert(_sample_rate_list_action);

    if (_sample_rate_list_action->isVisible())
	{
		const int index = _sample_rate_list.currentIndex();
		if (index >= 0)
			sample_rate = _sample_rate_list.itemData(
				index).value<uint64_t>();
	}

	if (sample_rate == 0)
		return;

    // Get last samplerate
    if (sr_config_get(_sdi->driver, SR_CONF_SAMPLERATE,
        &gvar, _sdi) != SR_OK) {
        qDebug() <<
                "WARNING: Failed to get value of sample rate";
        return;
    }
    last_sample_rate = g_variant_get_uint64(gvar);
    g_variant_unref(gvar);

	// Set the samplerate
    if (sr_config_set(_sdi, SR_CONF_SAMPLERATE,
        g_variant_new_uint64(sample_rate)) != SR_OK) {
		qDebug() << "Failed to configure samplerate.";
		return;
	}

    if (strcmp(_sdi->driver->name, "DSLogic") == 0 && _sdi->mode != DSO) {
        if ((last_sample_rate == SR_MHZ(200)&& sample_rate != SR_MHZ(200)) ||
            (last_sample_rate != SR_MHZ(200) && sample_rate == SR_MHZ(200)) ||
            (last_sample_rate == SR_MHZ(400)&& sample_rate != SR_MHZ(400)) ||
            (last_sample_rate != SR_MHZ(400) && sample_rate == SR_MHZ(400)))
            device_reload();
    }
}

void SamplingBar::on_sample_rate_changed()
{
	commit_sample_rate();
}

void SamplingBar::on_run_stop()
{
	commit_sample_rate();
	run_stop();
}

void SamplingBar::enable_toggle(bool enable)
{
    _record_length_selector.setDisabled(!enable);
    _sample_rate_list.setDisabled(!enable);
}

void SamplingBar::enable_run_stop(bool enable)
{
    _run_stop_button.setDisabled(!enable);
}

} // namespace toolbars
} // namespace pv
