/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
 * Copyright (C) 2013 Joel Holdsworth <joel@airwebreathe.org.uk>
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

#include <libsigrokdecode4DSL/libsigrokdecode.h>

#include "decodermenu.h"
#include <assert.h>

namespace pv {
namespace widgets {

DecoderMenu::DecoderMenu(QWidget *parent, bool first_level_decoder) :
	QMenu(parent),
	_mapper(this)
{
	GSList *l = g_slist_sort(g_slist_copy(
		(GSList*)srd_decoder_list()), decoder_name_cmp);
	for(; l; l = l->next)
	{
		const srd_decoder *const d = (srd_decoder*)l->data;
		assert(d);

		const bool have_probes = (d->channels || d->opt_channels) != 0;
		if (first_level_decoder == have_probes) {
			QAction *const action =
				addAction(QString::fromUtf8(d->name));
			action->setData(qVariantFromValue(l->data));
			_mapper.setMapping(action, action);
			connect(action, SIGNAL(triggered()),
				&_mapper, SLOT(map()));
		}
	}
	g_slist_free(l);

	connect(&_mapper, SIGNAL(mapped(QObject*)),
		this, SLOT(on_action(QObject*)));
}

int DecoderMenu::decoder_name_cmp(const void *a, const void *b)
{
	return strcmp(((const srd_decoder*)a)->name,
		((const srd_decoder*)b)->name);
}

void DecoderMenu::on_action(QObject *action)
{
	assert(action);
	srd_decoder *const dec =
		(srd_decoder*)((QAction*)action)->data().value<void*>();
	assert(dec);

    selected();
	decoder_selected(dec);	
}

} // widgets
} // pv
