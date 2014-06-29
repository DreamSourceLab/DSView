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


#ifndef DSLOGIC_PV_SIGSESSION_H
#define DSLOGIC_PV_SIGSESSION_H

#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/thread.hpp>

#include <string>
#include <utility>
#include <vector>

#include <QObject>
#include <QString>
#include <QLine>
#include <QVector>
#include <QMap>
#include <QVariant>

#include <libsigrok4DSLogic/libsigrok.h>
#include <libusb.h>

namespace pv {

class DeviceManager;

namespace data {
class Analog;
class AnalogSnapshot;
class Dso;
class DsoSnapshot;
class Logic;
class LogicSnapshot;
class Group;
class GroupSnapshot;
}

namespace view {
class Signal;
}

namespace decoder {
class Decoder;
class DecoderFactory;
}

class SigSession : public QObject
{
        Q_OBJECT

private:
    static const float Oversampling;

public:
	enum capture_state {
        Init,
		Stopped,
		Running
	};

public:
	SigSession(DeviceManager &device_manager);

	~SigSession();

	struct sr_dev_inst* get_device() const;

	/**
	 * Sets device instance that will be used in the next capture session.
	 */
    int set_device(struct sr_dev_inst *sdi);

	void release_device(struct sr_dev_inst *sdi);

	void load_file(const std::string &name,
		boost::function<void (const QString)> error_handler);

    void save_file(const std::string &name);

	capture_state get_capture_state() const;

	void start_capture(uint64_t record_length,
		boost::function<void (const QString)> error_handler);

	void stop_capture();

	std::vector< boost::shared_ptr<view::Signal> >
		get_signals();
    std::vector< boost::shared_ptr<view::Signal> >
        get_pro_signals();

    int get_logic_probe_cnt(const struct sr_dev_inst *sdi);
    int get_dso_probe_cnt(const struct sr_dev_inst *sdi);
    int get_analog_probe_cnt(const struct sr_dev_inst *sdi);

    void init_signals(const struct sr_dev_inst *sdi);

    void update_signals(const struct sr_dev_inst *sdi);

    void add_group();

    void del_group();

    void add_protocol(std::list<int> probe_index_list, decoder::Decoder *decoder);

    void del_protocol(int protocol_index);

    void del_signal(std::vector< boost::shared_ptr<view::Signal> >::iterator i);

	boost::shared_ptr<data::Logic> get_data();

    void* get_buf(int& unit_size, uint64_t& length);

    quint64 get_last_sample_rate() const;

    quint64 get_total_sample_len() const;
    void set_total_sample_len(quint64 length);

    QVector<std::pair<decoder::Decoder *, std::list<int> > > get_decoders() const;

    void add_protocol_analyzer(int decoder_index, std::list<int> _sel_probes,
                               QMap<QString, QVariant> &_options, QMap<QString, int> _options_index);
    void rst_protocol_analyzer(int rst_index, std::list<int> _sel_probes,
                               QMap<QString, QVariant> &_options, QMap<QString, int> _options_index);
    void del_protocol_analyzer(int protocol_index);

    std::list<int> get_decode_probes(int decode_index);
    QMap<QString, int> get_decode_options_index(int decode_index);

    void start_hotplug_proc(boost::function<void (const QString)> error_handler);
    void stop_hotplug_proc();
    void register_hotplug_callback();
    void deregister_hotplug_callback();

    void set_adv_trigger(bool adv_trigger);

    void start_dso_ctrl_proc(boost::function<void (const QString)> error_handler);
    void stop_dso_ctrl_proc();
    int set_dso_ctrl(int key);
    uint16_t get_dso_ch_num();

private:
	void set_capture_state(capture_state state);

private:
    // thread for sample/load
	void load_thread_proc(const std::string name,
		boost::function<void (const QString)> error_handler);
	void sample_thread_proc(struct sr_dev_inst *sdi,
		uint64_t record_length,
		boost::function<void (const QString)> error_handler);

    // data feed
	void feed_in_header(const sr_dev_inst *sdi);
	void feed_in_meta(const sr_dev_inst *sdi,
		const sr_datafeed_meta &meta);
    void feed_in_trigger(const ds_trigger_pos &trigger_pos);
	void feed_in_logic(const sr_datafeed_logic &logic);
    void feed_in_dso(const sr_datafeed_dso &dso);
	void feed_in_analog(const sr_datafeed_analog &analog);
	void data_feed_in(const struct sr_dev_inst *sdi,
		const struct sr_datafeed_packet *packet);
	static void data_feed_in_proc(const struct sr_dev_inst *sdi,
		const struct sr_datafeed_packet *packet, void *cb_data);

        void hotplug_proc(boost::function<void (const QString)> error_handler);
        static int hotplug_callback(struct libusb_context *ctx, struct libusb_device *dev, 
                         libusb_hotplug_event event, void *user_data);

	void dso_ctrl_proc(boost::function<void (const QString)> error_handler);

private:
	DeviceManager &_device_manager;

	/**
	 * The device instance that will be used in the next capture session.
	 */
	struct sr_dev_inst *_sdi;

	mutable boost::mutex _sampling_mutex;
	capture_state _capture_state;

	mutable boost::mutex _signals_mutex;
	std::vector< boost::shared_ptr<view::Signal> > _signals;

    decoder::DecoderFactory *_decoderFactory;
    QVector< std::pair<decoder::Decoder* , std::list<int> > > _decoders;
    std::vector< boost::shared_ptr<view::Signal> > _protocol_signals;

	mutable boost::mutex _data_mutex;
	boost::shared_ptr<data::Logic> _logic_data;
	boost::shared_ptr<data::LogicSnapshot> _cur_logic_snapshot;
    boost::shared_ptr<data::Dso> _dso_data;
    boost::shared_ptr<data::DsoSnapshot> _cur_dso_snapshot;
	boost::shared_ptr<data::Analog> _analog_data;
	boost::shared_ptr<data::AnalogSnapshot> _cur_analog_snapshot;
    boost::shared_ptr<data::Group> _group_data;
    boost::shared_ptr<data::GroupSnapshot> _cur_group_snapshot;
    int _group_cnt;
    int _protocol_cnt;

	std::auto_ptr<boost::thread> _sampling_thread;

    quint64 _last_sample_rate;

    quint64 _total_sample_len;

    libusb_hotplug_callback_handle _hotplug_handle;
    std::auto_ptr<boost::thread> _hotplug;
    bool _hot_attach;
    bool _hot_detach;

    bool _adv_trigger;

    bool _vDial_changed;
    bool _hDial_changed;
    uint16_t _dso_ctrl_channel;
    std::auto_ptr<boost::thread> _dso_ctrl_thread;

signals:
	void capture_state_changed(int state);

	void signals_changed();

	void data_updated();

    void sample_rate_changed(quint64 sample_rate);

    void receive_data(quint64 length);

    void device_attach();
    void device_detach();

    void test_data_error();

    void receive_trigger(quint64 trigger_pos);

    void dso_ch_changed(uint16_t num);

public slots:


private:
	// TODO: This should not be necessary. Multiple concurrent
	// sessions should should be supported and it should be
	// possible to associate a pointer with a ds_session.
	static SigSession *_session;
};

} // namespace pv

#endif // DSLOGIC_PV_SIGSESSION_H
