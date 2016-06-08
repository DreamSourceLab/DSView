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


#ifndef DSVIEW_PV_MEASUREDOCK_H
#define DSVIEW_PV_MEASUREDOCK_H

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
#include <QCheckBox>

#include <QVector>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>

#include <vector>

#include <libsigrok4DSL/libsigrok.h>

namespace pv {

class SigSession;

namespace view {
    class View;
}

namespace dock {

class MeasureDock : public QScrollArea
{
    Q_OBJECT

public:
    MeasureDock(QWidget *parent, pv::view::View &view, SigSession &session);
    ~MeasureDock();

    void paintEvent(QPaintEvent *);
signals:

private slots:
    void delta_update();
    void goto_cursor();

public slots:
    void cursor_update();
    void cursor_moved();
    void measure_updated();

private:
    SigSession &_session;
    view::View &_view;

    QWidget *_widget;
    QGridLayout *_mouse_layout;
    QGroupBox *_mouse_groupBox;
    QCheckBox *_fen_checkBox;
    QLabel *_width_label;
    QLabel *_period_label;
    QLabel *_freq_label;
    QLabel *_duty_label;

    QGridLayout *_cursor_layout;
    QGroupBox *_cursor_groupBox;
    QComboBox *_t1_comboBox;
    QComboBox *_t2_comboBox;
    QComboBox *_t3_comboBox;
    QLabel *_delta_label_t1t2;
    QLabel *_cnt_label_t1t2;
    QLabel *_delta_label_t2t3;
    QLabel *_cnt_label_t2t3;
    QLabel *_delta_label_t1t3;
    QLabel *_cnt_label_t1t3;
    int _t1_last_index;
    int _t2_last_index;
    int _t3_last_index;

    QVector <QPushButton *> _cursor_pushButton_list;
    QVector <QLabel *> _curpos_label_list;
    QVector <QLabel *> _space_label_list;
};

} // namespace dock
} // namespace pv

#endif // DSVIEW_PV_MEASUREDOCK_H
