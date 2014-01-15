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


#include "decoder.h"

namespace pv {
namespace decoder {

Decoder::Decoder(boost::shared_ptr<pv::data::Logic> data, std::list<int> sel_probes, QMap<QString, int> options_index) :
    _data(data),
    _sel_probes(sel_probes),
    _options_index(options_index),
    _total_state(0),
    _max_state_samples(0)
{
}

std::list<int > Decoder::get_probes()
{
    return _sel_probes;
}

QMap <QString, int> Decoder::get_options_index()
{
    return _options_index;
}

void Decoder::set_data(boost::shared_ptr<data::Logic> _logic_data)
{
    assert(_logic_data);
    _data = _logic_data;
}

} // namespace decoder
} // namespace pv
