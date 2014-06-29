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


#ifndef DSLOGIC_PV_DSOTRIGGERDOCK_H
#define DSLOGIC_PV_DSOTRIGGERDOCK_H

#include <QDockWidget>
#include <QSlider>
#include <QSpinBox>
#include <QButtonGroup>

#include <vector>

#include <libsigrok4DSLogic/libsigrok.h>

namespace pv {

class SigSession;

namespace dock {

class DsoTriggerDock : public QWidget
{
    Q_OBJECT

public:
    DsoTriggerDock(QWidget *parent, SigSession &session);
    ~DsoTriggerDock();

    void paintEvent(QPaintEvent *);

    void device_change();

signals:

private slots:
    void pos_changed(int pos);
    void source_changed();
    void type_changed();

private:

private:
    SigSession &_session;

    QSpinBox *position_spinBox;
    QSlider *position_slider;

    QButtonGroup *source_group;
    QButtonGroup *type_group;
};

} // namespace dock
} // namespace pv

#endif // DSLOGIC_PV_DSOTRIGGERDOCK_H
