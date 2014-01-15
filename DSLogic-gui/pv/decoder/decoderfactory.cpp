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

#include "decoderfactory.h"
#include "dsi2c.h"
#include "dsspi.h"
#include "dsserial.h"
#include "dsdmx512.h"
#include "ds1wire.h"

namespace pv {
namespace decoder {

DecoderFactory::DecoderFactory()
{
}

Decoder* DecoderFactory::createDecoder(int type, boost::shared_ptr<data::Logic> data,
                                       std::list <int > _sel_probes, QMap <QString, QVariant> &_options, QMap <QString, int> _options_index)
{
    Decoder *decoder = NULL;

    switch(type)
    {
    case I2C:
        decoder = new dsI2c(data, _sel_probes, _options, _options_index);
        break;
    case SPI:
        decoder = new dsSpi(data, _sel_probes, _options, _options_index);
        break;
    case Serial:
        decoder = new dsSerial(data, _sel_probes, _options, _options_index);
        break;
    case Dmx512:
        decoder = new dsDmx512(data, _sel_probes, _options, _options_index);
        break;
    case Wire1:
        decoder = new ds1Wire(data, _sel_probes, _options, _options_index);
        break;
    default:
        decoder = new dsI2c(data, _sel_probes, _options, _options_index);
    }

    return decoder;
}

} // namespace decoder
} // namespace pv
