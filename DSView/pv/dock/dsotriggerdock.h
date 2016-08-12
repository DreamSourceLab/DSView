/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
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


#ifndef DSVIEW_PV_DSOTRIGGERDOCK_H
#define DSVIEW_PV_DSOTRIGGERDOCK_H

#include <QDockWidget>
#include <QSlider>
#include <QSpinBox>
#include <QButtonGroup>
#include <QScrollArea>
#include <QComboBox>

#include <vector>

#include <libsigrok4DSL/libsigrok.h>

namespace pv {

class SigSession;

namespace dock {

class DsoTriggerDock : public QScrollArea
{
    Q_OBJECT

public:
    DsoTriggerDock(QWidget *parent, SigSession &session);
    ~DsoTriggerDock();

    void paintEvent(QPaintEvent *);

    void device_change();

    void init();

signals:
    void set_trig_pos(int percent);

private slots:
    void pos_changed(int pos);
    void hold_changed(int hold);
    void margin_changed(int margin);
    void source_changed();
    void type_changed();
    void channel_changed(int ch);

private:

private:
    SigSession &_session;

    QWidget *_widget;

    QComboBox *holdoff_comboBox;
    QSpinBox *holdoff_spinBox;
    QSlider *holdoff_slider;

    QSlider *margin_slider;

    QSpinBox *position_spinBox;
    QSlider *position_slider;

    QButtonGroup *source_group;
    QComboBox *channel_comboBox;
    QButtonGroup *type_group;
};

} // namespace dock
} // namespace pv

#endif // DSVIEW_PV_DSOTRIGGERDOCK_H
