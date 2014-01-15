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


#ifndef DSLOGIC_PV_DECODER_H
#define DSLOGIC_PV_DECODER_H

#include <boost/shared_ptr.hpp>

#include <QColor>
#include <QMap>
#include <QVariant>

#include <stdint.h>
#include <vector>

namespace pv {

namespace data {
class Logic;
}

namespace decoder {

enum {
    DEC_CMD = 0,
    DEC_DATA = 1,
    DEC_CNT = 2,
    DEC_NODETAIL
};

struct ds_view_state {
    uint64_t index;
    uint64_t samples;
    uint16_t type;
    uint8_t state;
    uint8_t data;
};

enum {
    I2C = 0,
    SPI,
    Serial,
    Dmx512,
    Wire1
};

static QString protocol_list[] = {
    "I2C",
    "SPI",
    "Serial",
    "DMX512",
    "1-Wire",
    NULL,
};

class Decoder
{
protected:
    static const int _view_scale = 8;
    Decoder(boost::shared_ptr<pv::data::Logic> data, std::list <int > sel_probes, QMap <QString, int> options_index);
public:
    std::list<int > get_probes();
    QMap <QString, int> get_options_index();
    void set_data(boost::shared_ptr<data::Logic> _logic_data);

public:
    virtual QString get_decode_name() = 0;

    virtual void recode(std::list <int > sel_probes, QMap <QString, QVariant>& options, QMap <QString, int> options_index) = 0;
    virtual void decode() = 0;

    virtual void fill_color_table(std::vector <QColor>& _color_table) = 0;

    virtual void fill_state_table(std::vector <QString>& _state_table) = 0;

    virtual void get_subsampled_states(std::vector<struct ds_view_state> &states,
                                       uint64_t start, uint64_t end,
                                       float min_length) = 0;

private:


protected:

    boost::shared_ptr<pv::data::Logic> _data;
    std::list <int > _sel_probes;
    QMap <QString, int> _options_index;
    uint64_t _total_state;
    uint64_t _max_state_samples;
};

} // namespace decoder
} // namespace pv

#endif // DSLOGIC_PV_DECODER_H
