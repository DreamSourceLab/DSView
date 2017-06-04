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

#include <cassert>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "inputfile.h"

using std::string;

namespace pv {
namespace device {

InputFile::InputFile(QString path) :
	File(path),
	_input(NULL)
{
}

sr_dev_inst* InputFile::dev_inst() const
{
	assert(_input);
	return _input->sdi;
}

void InputFile::use(SigSession *owner) throw(QString)
{
    (void)owner;
	assert(!_input);

    // only *.dsl file is valid
    // don't allow other types of file input
    throw tr("Not a valid DSView data file.");
    return;

//	_input = load_input_file_format(_path, NULL);
//	File::use(owner);

//	sr_session_new();

//	if (sr_session_dev_add(_input->sdi) != SR_OK)
//		throw tr("Failed to add session device.");
}

void InputFile::release()
{
	if (!_owner)
		return;

	assert(_input);
	File::release();
	sr_dev_close(_input->sdi);
	sr_session_destroy();
	_input = NULL;
}

sr_input_format* InputFile::determine_input_file_format(const QString filename)
{
	int i;

	/* If there are no input formats, return NULL right away. */
	sr_input_format *const *const inputs = sr_input_list();
	if (!inputs) {
        g_critical("No supported input formats available.");
		return NULL;
	}

	/* Otherwise, try to find an input module that can handle this file. */
	for (i = 0; inputs[i]; i++) {
        if (inputs[i]->format_match(filename.toLocal8Bit().data()))
			break;
	}

	/* Return NULL if no input module wanted to touch this. */
	if (!inputs[i]) {
        g_critical("Error: no matching input module found.");
		return NULL;
	}

	return inputs[i];
}

sr_input* InputFile::load_input_file_format(const QString filename,
	sr_input_format *format)
{
	struct stat st;
	sr_input *in;

	if (!format && !(format =
        determine_input_file_format(filename))) {
		/* The exact cause was already logged. */
		throw tr("Failed to load file");
	}

    if (stat(filename.toLocal8Bit().data(), &st) == -1)
		throw tr("Failed to load file");

	/* Initialize the input module. */
	if (!(in = new sr_input)) {
		throw tr("Failed to allocate input module.");
	}

	in->format = format;
	in->param = NULL;
	if (in->format->init &&
        in->format->init(in, filename.toLocal8Bit().data()) != SR_OK) {
		throw tr("Failed to load file");
	}

	return in;
}

void InputFile::start()
{
}

void InputFile::run()
{
	assert(_input);
	assert(_input->format);
	assert(_input->format->loadfile);
    _input->format->loadfile(_input, _path.toLocal8Bit().data());
}

} // device
} // pv
