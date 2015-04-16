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


#ifndef DSLOGIC_PV_TRIGGERDOCK_H
#define DSLOGIC_PV_TRIGGERDOCK_H

#include <QDockWidget>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QRadioButton>
#include <QSlider>
#include <QLineEdit>
#include <QSpinBox>
#include <QGroupBox>
#include <QTableWidget>

#include <QVector>
#include <QVBoxLayout>
#include <QHBoxLayout>

#include <vector>

#include <libsigrok4DSLogic/libsigrok.h>

namespace pv {

class SigSession;

namespace dock {

class TriggerDock : public QWidget
{
    Q_OBJECT

public:
    TriggerDock(QWidget *parent, SigSession &session);
    ~TriggerDock();

    void paintEvent(QPaintEvent *);

    void device_change();

signals:

private slots:
    void simple_trigger();
    void adv_trigger();
    void trigger_stages_changed(int index);
    void widget_enable();

    void value_changed();
    void logic_changed(int index);
    void count_changed();
    void inv_changed(int index);

    void pos_changed(int pos);

private:

private:
    SigSession &_session;

    QRadioButton *simple_radioButton;
    QRadioButton *adv_radioButton;

    QLabel *position_label;
    QSpinBox *position_spinBox;
    QSlider *position_slider;

    QLabel *stages_label;
    QComboBox *stages_comboBox;

    QTabWidget *stage_tabWidget;

    QVector <QGroupBox *> _stage_groupBox_list;
    QVector <QLabel *>    _mu_label_list;
    QVector <QComboBox *> _logic_comboBox_list;
    QVector <QLineEdit *> _value0_lineEdit_list;
    QVector <QSpinBox *> _count0_spinBox_list;
    QVector <QComboBox *> _inv0_comboBox_list;
    QVector <QLineEdit *> _value1_lineEdit_list;
    QVector <QSpinBox *> _count1_spinBox_list;
    QVector <QComboBox *> _inv1_comboBox_list;
};

} // namespace dock
} // namespace pv

#endif // DSLOGIC_PV_TRIGGERDOCK_H
