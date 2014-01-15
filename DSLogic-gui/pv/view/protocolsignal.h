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


#ifndef DSLOGIC_PV_PROTOCOLSIGNAL_H
#define DSLOGIC_PV_PROTOCOLSIGNAL_H

#include "signal.h"
#include "../decoder/decoder.h"

#include <vector>

#include <boost/shared_ptr.hpp>

namespace pv {

namespace data {
class Logic;
}
namespace decoder {
class Decoder;
}

namespace view {

class ProtocolSignal : public Signal
{
private:
    static const int StateHeight;
    static const int StateRound;

public:
    ProtocolSignal(QString name,
                   boost::shared_ptr<pv::data::Logic> data,
                   pv::decoder::Decoder *decoder,
                   std::list<int> index_list, int order, int protocol_index);

    virtual ~ProtocolSignal();

    void set_data(boost::shared_ptr<pv::data::Logic> _logic_data,
                  boost::shared_ptr<pv::data::Analog> _analog_data,
                  boost::shared_ptr<pv::data::Group> _group_data);

    void paint(QPainter &p, int y, int left, int right, double scale,
        double offset);

    const std::vector< std::pair<uint64_t, bool> > cur_edges() const;

    void set_decoder(pv::decoder::Decoder *decoder);

    pv::decoder::Decoder* get_decoder();

    void del_decoder();

private:
    std::list<int> _probe_index_list;
    boost::shared_ptr<pv::data::Logic> _data;
    pv::decoder::Decoder * _decoder;
    std::vector<pv::decoder::ds_view_state > _cur_states;
    std::vector< QColor > _color_table;
    std::vector< QString > _state_table;
};

} // namespace view
} // namespace pv

#endif // DSLOGIC_PV_PROTOCOLSIGNAL_H
