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


#ifndef DSVIEW_PV_TRIGGERDOCK_H
#define DSVIEW_PV_TRIGGERDOCK_H

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
#include <QJsonObject>

#include <QVector>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>

#include <vector>

#include <libsigrok4DSL/libsigrok.h>

namespace pv {

class SigSession;

namespace dock {

class TriggerDock : public QScrollArea
{
    Q_OBJECT

private:
    static const int MinTrigPosition;

public:
    TriggerDock(QWidget *parent, SigSession &session);
    ~TriggerDock();

    void paintEvent(QPaintEvent *);

    void init();

    QJsonObject get_session();
    void set_session(QJsonObject ses);

    /*
     * commit trigger setting
     * return 0: simple trigger
     *        1: advanced trigger
     */
    bool commit_trigger();

signals:

public slots:
    void simple_trigger();
    void adv_trigger();
    void widget_enable(int index);

    void value_changed();

    void device_change();

private:

private:
    SigSession &_session;

    QWidget *_widget;

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

    QTabWidget *_adv_tabWidget;
    QGroupBox *_serial_groupBox;
    QLabel *_serial_start_label;
    QLineEdit *_serial_start_lineEdit;
    QLabel *_serial_stop_label;
    QLineEdit *_serial_stop_lineEdit;
    QLabel *_serial_edge_label;
    QLineEdit *_serial_edge_lineEdit;
    QLabel *_serial_data_lable;
    QComboBox *_serial_data_comboBox;
    QLabel *_serial_value_lable;
    QLineEdit *_serial_value_lineEdit;
    QLabel *_serial_vcnt_lable;
    QSpinBox *_serial_vcnt_spinBox;
};

} // namespace dock
} // namespace pv

#endif // DSVIEW_PV_TRIGGERDOCK_H
