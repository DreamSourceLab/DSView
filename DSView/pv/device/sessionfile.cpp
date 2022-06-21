/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
 * Copyright (C) 2014 Joel Holdsworth <joel@airwebreathe.org.uk>
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

#include "sessionfile.h"
#include <assert.h>
#include "../utility/path.h"

namespace pv {
namespace device {

SessionFile::SessionFile(QString path) :
	File(path)
{
	_sdi = NULL;
	_is_file = true;
}

sr_dev_inst* SessionFile::dev_inst()
{
	return _sdi;
}

void SessionFile::use(SigSession *owner)
{ 
	if (_sdi){
        release_source();
	}

	auto f_name = path::ConvertPath(_path);

    if (sr_session_load(f_name.c_str()) != SR_OK)
		throw tr("Failed to open file.\n");

	GSList *devlist = NULL;
	sr_session_dev_list(&devlist);

	if (!devlist || !devlist->data) {
		if (devlist)
			g_slist_free(devlist);
		throw tr("Failed to start session.");
	}

	_sdi = (sr_dev_inst*)devlist->data;
	g_slist_free(devlist);

	File::use(owner);
}

void SessionFile::release()
{  
   release_source();
   File::release();
}

void SessionFile::release_source()
{
	if (_sdi != NULL)
	{ 
		sr_dev_close(_sdi);
		sr_dev_clear(_sdi->driver);
		sr_session_destroy();
		_sdi = NULL;
	}
}

} // device
} // pv
