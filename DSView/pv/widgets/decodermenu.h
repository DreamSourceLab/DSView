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

#ifndef DSVIEW_PV_WIDGETS_DECODERMENU_H
#define DSVIEW_PV_WIDGETS_DECODERMENU_H

#include <QMenu>
#include <QSignalMapper>

struct srd_decoder;

namespace pv {
namespace widgets {

class DecoderMenu : public QMenu
{
	Q_OBJECT;

public:
	DecoderMenu(QWidget *parent, bool first_level_decoder = false);

private:
	static int decoder_name_cmp(const void *a, const void *b);


private slots:
	void on_action(QObject *action);

signals:
	void decoder_selected(srd_decoder *decoder);
    void selected();

private:
	QSignalMapper _mapper;
};

} // widgets
} // pv

#endif // DSVIEW_PV_WIDGETS_DECODERMENU_H
