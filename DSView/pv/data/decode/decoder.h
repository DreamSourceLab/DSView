/*
 * This file is part of the PulseView project.
 * DSView is based on PulseView.
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
#include <string>
#include <sstream>
#include <iostream>
  
#include <glib.h>

struct srd_decoder;
struct srd_decoder_inst;
struct srd_channel;
struct srd_session;

namespace pv {
namespace data {
namespace decode {

class Decoder
{
public:
	Decoder(const srd_decoder *const decoder);

public: 

	virtual ~Decoder();

	inline const srd_decoder* decoder(){
        return _decoder;
    }

	inline bool shown(){
        return _shown;
    }

	inline void show(bool show = true){
         _shown = show;
    }

    inline std::map<const srd_channel*, int>& channels(){
        return _probes;
    }

    void set_probes(std::map<const srd_channel*, int> probes);

	inline std::map<std::string, GVariant*>& options(){
        return _options;
    }

	void set_option(const char *id, GVariant *value);

	bool have_required_probes();

    srd_decoder_inst* create_decoder_inst(srd_session *session);

    void set_decode_region(uint64_t start, uint64_t end);

    inline uint64_t decode_start(){
        return _decode_start;
    }

    inline void reset_start(){
        _decode_start = _decode_start_back;
    }

    inline uint64_t decode_end(){
        return _decode_end;
    }

    bool commit();

    inline int get_channel_type(const srd_channel* ch){
        return ch->type;
    }

    inline const srd_decoder* get_dec_handel(){
        return _decoder;
    }

private:
	const srd_decoder *const _decoder;
 
    std::map<const srd_channel*, int>   _probes;
	std::map<std::string, GVariant*>    _options;
    std::map<const srd_channel*, int>   _probes_back;
    std::map<std::string, GVariant*>    _options_back;

    uint64_t        _decode_start;
    uint64_t        _decode_end;
    uint64_t        _decode_start_back;
    uint64_t        _decode_end_back;

    bool            _setted;
    bool            _shown;
};

} // namespace decode
} // namespace data
} // namespace pv

#endif // DSVIEW_PV_DATA_DECODE_DECODER_H
