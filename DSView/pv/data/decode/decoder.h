/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2013 Joel Holdsworth <joel@airwebreathe.org.uk>
 * Copyright (C) 2014 DreamSourceLab <support@dreamsourcelab.com>
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

#ifndef DSVIEW_PV_DATA_DECODE_DECODER_H
#define DSVIEW_PV_DATA_DECODE_DECODER_H

#include <map>
#include <set>

#include <boost/shared_ptr.hpp>

#include <glib.h>

struct srd_decoder;
struct srd_decoder_inst;
struct srd_channel;
struct srd_session;

namespace pv {

namespace view {
class LogicSignal;
}

namespace data {

class Logic;

namespace decode {

class Decoder
{
public:
	Decoder(const srd_decoder *const decoder);

	virtual ~Decoder();

	const srd_decoder* decoder() const;

	bool shown() const;
	void show(bool show = true);

	const std::map<const srd_channel*,
		boost::shared_ptr<view::LogicSignal> >& channels() const;
	void set_probes(std::map<const srd_channel*,
		boost::shared_ptr<view::LogicSignal> > probes);

	const std::map<std::string, GVariant*>& options() const;

	void set_option(const char *id, GVariant *value);

	bool have_required_probes() const;

	srd_decoder_inst* create_decoder_inst(
		srd_session *session, int unit_size) const;

	std::set< boost::shared_ptr<pv::data::Logic> > get_data();	

    void set_decode_region(uint64_t start, uint64_t end);
    uint64_t decode_start() const;
    uint64_t decode_end() const;

    bool commit();

private:
	const srd_decoder *const _decoder;

	bool _shown;

    std::map<const srd_channel*, boost::shared_ptr<pv::view::LogicSignal> >
		_probes;
	std::map<std::string, GVariant*> _options;

    std::map<const srd_channel*, boost::shared_ptr<pv::view::LogicSignal> >
        _probes_back;
    std::map<std::string, GVariant*> _options_back;

    uint64_t _decode_start, _decode_end;
    uint64_t _decode_start_back, _decode_end_back;

    bool _setted;
};

} // namespace decode
} // namespace data
} // namespace pv

#endif // DSVIEW_PV_DATA_DECODE_DECODER_H
