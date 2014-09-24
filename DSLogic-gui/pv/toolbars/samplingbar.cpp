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

#include "../devicemanager.h"
#include "../device/devinst.h"
#include "../dialogs/deviceoptions.h"

using boost::shared_ptr;
using std::map;
using std::max;
using std::min;
using std::string;

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

SamplingBar::SamplingBar(SigSession &session, QWidget *parent) :
	QToolBar("Sampling Bar", parent),
    _session(session),
    _enable(true),
    _device_selector(this),
    _updating_device_selector(false),
    _configure_button(this),
    _sample_count(this),
    _sample_rate(this),
    _updating_sample_rate(false),
    _updating_sample_count(false),
    _icon_stop(":/icons/stop.png"),
    _icon_start(":/icons/start.png"),
    _icon_instant(":/icons/instant.png"),
    _run_stop_button(this),
    _instant_button(this),
    _instant(false)
{
    setMovable(false);

    connect(&_device_selector, SIGNAL(currentIndexChanged (int)),
        this, SLOT(on_device_selected()));
    connect(&_configure_button, SIGNAL(clicked()),
        this, SLOT(on_configure()));
	connect(&_run_stop_button, SIGNAL(clicked()),
		this, SLOT(on_run_stop()));
    connect(&_instant_button, SIGNAL(clicked()),
        this, SLOT(on_instant_stop()));

    _configure_button.setIcon(QIcon::fromTheme("configure",
        QIcon(":/icons/params.png")));
    _run_stop_button.setIcon(_icon_start);
    _instant_button.setIcon(_icon_instant);

//	for (size_t i = 0; i < countof(RecordLengths); i++)
//	{
//		const uint64_t &l = RecordLengths[i];
//		char *const text = ds_si_string_u64(l, " samples");
//		_sample_count.addItem(QString(text),
//			qVariantFromValue(l));
//		g_free(text);

//		if (l == DefaultRecordLength)
//			_sample_count.setCurrentIndex(i);
//	}
    _sample_count.setSizeAdjustPolicy(QComboBox::AdjustToContents);
	set_sampling(false);

    //_run_stop_button.setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    _run_stop_button.setObjectName(tr("run_stop_button"));

    connect(&_sample_rate, SIGNAL(currentIndexChanged(int)),
        this, SLOT(on_samplerate_sel(int)));

    addWidget(new QLabel(tr(" ")));
    addWidget(&_device_selector);
    addWidget(&_configure_button);
    addWidget(&_sample_count);
    addWidget(new QLabel(tr(" @ ")));
    addWidget(&_sample_rate);
	addWidget(&_run_stop_button);
    addWidget(&_instant_button);
}

void SamplingBar::set_device_list(
    const std::list< shared_ptr<pv::device::DevInst> > &devices,
    shared_ptr<pv::device::DevInst> selected)
{
    int selected_index = -1;

    assert(selected);

    _updating_device_selector = true;

    _device_selector.clear();
    _device_selector_map.clear();

    BOOST_FOREACH (shared_ptr<pv::device::DevInst> dev_inst, devices) {
        assert(dev_inst);
        const string title = dev_inst->format_device_title();
        const void *id = dev_inst->get_id();
        assert(id);

        if (selected == dev_inst)
            selected_index = _device_selector.count();

        _device_selector_map[id] = dev_inst;
        _device_selector.addItem(title.c_str(),
            qVariantFromValue((void*)id));
    }

    // The selected device should have been in the list
    assert(selected_index != -1);
    _device_selector.setCurrentIndex(selected_index);

    update_sample_rate_selector();
    update_sample_count_selector();
    update_scale();

    _updating_device_selector = false;
}

shared_ptr<pv::device::DevInst> SamplingBar::get_selected_device() const
{
    const int index = _device_selector.currentIndex();
    if (index < 0)
        return shared_ptr<pv::device::DevInst>();

    const void *const id =
        _device_selector.itemData(index).value<void*>();
    assert(id);

    map<const void*, boost::weak_ptr<device::DevInst> >::
        const_iterator iter = _device_selector_map.find(id);
    if (iter == _device_selector_map.end())
        return shared_ptr<pv::device::DevInst>();

    return shared_ptr<pv::device::DevInst>((*iter).second);
}

void SamplingBar::on_configure()
{
    int  ret;
    shared_ptr<pv::device::DevInst> dev_inst = get_selected_device();
    assert(dev_inst);

    pv::dialogs::DeviceOptions dlg(this, dev_inst->dev_inst());
    ret = dlg.exec();
    if (ret == QDialog::Accepted)
        device_updated();
}

uint64_t SamplingBar::get_record_length() const
{
    const int index = _sample_count.currentIndex();
	if (index < 0)
		return 0;

    return _sample_count.itemData(index).value<uint64_t>();
}

void SamplingBar::set_record_length(uint64_t length)
{
    for (int i = 0; i < _sample_count.count(); i++) {
        if (length == _sample_count.itemData(
            i).value<uint64_t>()) {
            _sample_count.setCurrentIndex(i);
            break;
        }
    }
}

void SamplingBar::set_sampling(bool sampling)
{
    if (_instant)
        _instant_button.setIcon(sampling ? _icon_stop : _icon_instant);
    else
        _run_stop_button.setIcon(sampling ? _icon_stop : _icon_start);

    if (!sampling) {
        _run_stop_button.setEnabled(true);
        _instant_button.setEnabled(true);
    }
}

void SamplingBar::set_sample_rate(uint64_t sample_rate)
{
    for (int i = _sample_rate.count() - 1; i >= 0; i--) {
        uint64_t cur_index_sample_rate = _sample_rate.itemData(
                    i).value<uint64_t>();
        if (sample_rate >= cur_index_sample_rate) {
            _sample_rate.setCurrentIndex(i);
            // commit the samplerate
            commit_sample_rate();
            break;
        }
    }
}

void SamplingBar::set_sample_limit(uint64_t sample_limit)
{
    for (int i = 0; i < _sample_count.count(); i++) {
        uint64_t cur_index_sample_limit = _sample_count.itemData(
                    i).value<uint64_t>();
        if (sample_limit <= cur_index_sample_limit) {
            _sample_count.setCurrentIndex(i);
            // commit the samplecount
            commit_sample_count();
            break;
        }
    }
}

void SamplingBar::update_sample_rate_selector()
{
	GVariant *gvar_dict, *gvar_list;
	const uint64_t *elements = NULL;
	gsize num_elements;

    if (_updating_sample_rate)
        return;

    const shared_ptr<device::DevInst> dev_inst = get_selected_device();
    if (!dev_inst)
        return;

    assert(!_updating_sample_rate);
    _updating_sample_rate = true;

    if (!(gvar_dict = dev_inst->list_config(NULL, SR_CONF_SAMPLERATE)))
    {
        _sample_rate.clear();
        _sample_rate.show();
        _updating_sample_rate = false;
        return;
    }

    if ((gvar_list = g_variant_lookup_value(gvar_dict,
			"samplerates", G_VARIANT_TYPE("at"))))
	{
		elements = (const uint64_t *)g_variant_get_fixed_array(
				gvar_list, &num_elements, sizeof(uint64_t));
        _sample_rate.clear();

		for (unsigned int i = 0; i < num_elements; i++)
		{
			char *const s = sr_samplerate_string(elements[i]);
            _sample_rate.addItem(QString(s),
				qVariantFromValue(elements[i]));
			g_free(s);
		}

        _sample_rate.show();
		g_variant_unref(gvar_list);
	}
    _updating_sample_rate = false;

	g_variant_unref(gvar_dict);
    update_sample_rate_selector_value();
}

void SamplingBar::update_sample_rate_selector_value()
{
    if (_updating_sample_rate)
        return;

    const uint64_t samplerate = get_selected_device()->get_sample_rate();

    assert(!_updating_sample_rate);
    _updating_sample_rate = true;

    for (int i = 0; i < _sample_rate.count(); i++)
        if (samplerate == _sample_rate.itemData(
            i).value<uint64_t>())
            _sample_rate.setCurrentIndex(i);

    _updating_sample_rate = false;
}

void SamplingBar::commit_sample_rate()
{
	uint64_t sample_rate = 0;
    uint64_t last_sample_rate = 0;

    if (_updating_sample_rate)
        return;

    assert(!_updating_sample_rate);
    _updating_sample_rate = true;

    const int index = _sample_rate.currentIndex();
    if (index >= 0)
        sample_rate = _sample_rate.itemData(
            index).value<uint64_t>();

	if (sample_rate == 0)
		return;

    // Get last samplerate
    last_sample_rate = get_selected_device()->get_sample_rate();

	// Set the samplerate
    get_selected_device()->set_config(NULL, NULL,
                                      SR_CONF_SAMPLERATE,
                                      g_variant_new_uint64(sample_rate));

    if (last_sample_rate != sample_rate)
        update_scale();

    _updating_sample_rate = false;
}

void SamplingBar::on_samplerate_sel(int index)
{
    uint64_t sample_rate = 0;
    uint64_t last_sample_rate = 0;

    if (index >= 0)
        sample_rate = _sample_rate.itemData(
            index).value<uint64_t>();

    const sr_dev_inst* _sdi = get_selected_device()->dev_inst();
    assert(_sdi);

    // Get last samplerate
    last_sample_rate = get_selected_device()->get_sample_rate();

    if (strcmp(_sdi->driver->name, "DSLogic") == 0 && _sdi->mode != DSO) {
        if ((last_sample_rate == SR_MHZ(200)&& sample_rate != SR_MHZ(200)) ||
            (last_sample_rate != SR_MHZ(200) && sample_rate == SR_MHZ(200)) ||
            (last_sample_rate == SR_MHZ(400)&& sample_rate != SR_MHZ(400)) ||
            (last_sample_rate != SR_MHZ(400) && sample_rate == SR_MHZ(400))) {

            // Set the samplerate
            get_selected_device()->set_config(NULL, NULL,
                                              SR_CONF_SAMPLERATE,
                                              g_variant_new_uint64(sample_rate));
            device_updated();
            update_scale();
        }
    }
}

void SamplingBar::update_sample_count_selector()
{
    GVariant *gvar_dict, *gvar_list;
    const uint64_t *elements = NULL;
    gsize num_elements;

    if (_updating_sample_count)
        return;

    const shared_ptr<device::DevInst> dev_inst = get_selected_device();
    if (!dev_inst)
        return;

    assert(!_updating_sample_count);
    _updating_sample_count = true;

    if (!(gvar_dict = dev_inst->list_config(NULL, SR_CONF_LIMIT_SAMPLES)))
    {
        _sample_count.clear();
        _sample_count.show();
        _updating_sample_count = false;
        return;
    }

    if ((gvar_list = g_variant_lookup_value(gvar_dict,
            "samplecounts", G_VARIANT_TYPE("at"))))
    {
        elements = (const uint64_t *)g_variant_get_fixed_array(
                gvar_list, &num_elements, sizeof(uint64_t));
        _sample_count.clear();

        for (unsigned int i = 0; i < num_elements; i++)
        {
            char *const s = sr_samplecount_string(elements[i]);
            _sample_count.addItem(QString(s),
                qVariantFromValue(elements[i]));
            g_free(s);
        }

        _sample_count.show();
        g_variant_unref(gvar_list);
    }

    _updating_sample_count = false;

    g_variant_unref(gvar_dict);
    update_sample_count_selector_value();
}

void SamplingBar::update_sample_count_selector_value()
{
    if (_updating_sample_count)
        return;

    const uint64_t samplecount = get_selected_device()->get_sample_limit();

    assert(!_updating_sample_count);
    _updating_sample_count = true;

    for (int i = 0; i < _sample_count.count(); i++)
        if (samplecount == _sample_count.itemData(
            i).value<uint64_t>())
            _sample_count.setCurrentIndex(i);

    _updating_sample_count = false;
}

void SamplingBar::commit_sample_count()
{
    uint64_t sample_count = 0;
    uint64_t last_sample_count = 0;

    if (_updating_sample_count)
        return;

    assert(!_updating_sample_count);
    _updating_sample_count = true;


    const int index = _sample_count.currentIndex();
    if (index >= 0)
        sample_count = _sample_count.itemData(
            index).value<uint64_t>();

    if (sample_count == 0)
        return;

    // Get last samplecount
    last_sample_count = get_selected_device()->get_sample_limit();

    // Set the samplecount
    get_selected_device()->set_config(NULL, NULL,
                                      SR_CONF_LIMIT_SAMPLES,
                                      g_variant_new_uint64(sample_count));

    _updating_sample_count = false;

    if (sample_count != last_sample_count)
        update_scale();
}

void SamplingBar::on_run_stop()
{
	commit_sample_rate();
    commit_sample_count();
    _instant = false;
	run_stop();
}

void SamplingBar::on_instant_stop()
{
    commit_sample_rate();
    commit_sample_count();
    _instant = true;
    instant_stop();
}

void SamplingBar::on_device_selected()
{
    if (_updating_device_selector)
        return;

    const shared_ptr<device::DevInst> dev_inst = get_selected_device();
    if (!dev_inst)
        return;

    _session.set_device(dev_inst);

    device_selected();
}

void SamplingBar::enable_toggle(bool enable)
{
    _sample_count.setDisabled(!enable);
    _sample_rate.setDisabled(!enable);
}

void SamplingBar::enable_run_stop(bool enable)
{
    _run_stop_button.setDisabled(!enable);
}

void SamplingBar::enable_instant(bool enable)
{
    _instant_button.setDisabled(!enable);
}

} // namespace toolbars
} // namespace pv
