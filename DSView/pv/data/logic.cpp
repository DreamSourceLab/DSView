/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
 * Copyright (C) 2012 Joel Holdsworth <joel@airwebreathe.org.uk>
 * Copyright (C) 2013 DreamSourceLab <support@dreamsourcelab.com>
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


#include "logic.h"
#include "logicsnapshot.h"

#include <boost/foreach.hpp>

using namespace boost;
using namespace std;

namespace pv {
namespace data {

Logic::Logic() :
    SignalData()
{
}

void Logic::push_snapshot(
	boost::shared_ptr<LogicSnapshot> &snapshot)
{
	_snapshots.push_front(snapshot);
}

deque< boost::shared_ptr<LogicSnapshot> >& Logic::get_snapshots()
{
	return _snapshots;
}

void Logic::clear()
{
    //_snapshots.clear();
    BOOST_FOREACH(const boost::shared_ptr<LogicSnapshot> s, _snapshots)
        s->clear();
}

void Logic::init()
{
    //_snapshots.clear();
    BOOST_FOREACH(const boost::shared_ptr<LogicSnapshot> s, _snapshots)
        s->init();
}

} // namespace data
} // namespace pv
