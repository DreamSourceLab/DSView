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

#include "../ui/dscombobox.h"
#include "../interface/icallbacks.h"

namespace pv {

class SigSession;

namespace view {
    class Cursor;
    class View;
}

namespace dock {

struct cursor_distance_info
{
    QWidget         *row_pannel;
    QToolButton     *del_bt;
    QPushButton     *start_bt;
    QPushButton     *end_bt;
    QLabel          *r_lable;
};

struct cursor_edge_info
{
    QWidget         *row_pannel;
    QToolButton     *del_bt;
    QPushButton     *start_bt;
    QPushButton     *end_bt;
    QLabel          *rising_edges_label;
    QComboBox       *box;
};

struct cursor_opt_info
{ 
    QToolButton     *del_bt;
    QPushButton     *goto_bt;
    QLabel          *info_lable;
    view::Cursor    *cursor;
};

class MeasureDock : public QScrollArea, public IFontForm
{
    Q_OBJECT

private:
    static const int Max_Measure_Limits = 16;

public:
    MeasureDock(QWidget *parent, pv::view::View &view, SigSession *session);
    ~MeasureDock();

    void reload();

private:
    void changeEvent(QEvent *event);
    void retranslateUi();
    void reStyle();

    //IFontForm
    void update_font();

private:
    QComboBox* create_probe_selector(QWidget *parent);
    void update_probe_selector(QComboBox *selector);

private slots:
    void goto_cursor();
    void del_dist_measure();
    void add_edge_measure();
    void del_edge_measure();
    void show_all_coursor();
    void set_se_cursor();
    const view::Cursor* find_cousor(int index);
    void update_dist();
    void update_edge();
    void set_cursor_btn_color(QPushButton *btn);
    void del_cursor();
    void add_dist_measure();

public slots:   
    void cursor_update();
    void cursor_moving();
    void reCalc();
    void measure_updated();
    void refresh();

private:
    SigSession *_session;
    view::View &_view;

    QWidget *_widget;
    QGridLayout *_mouse_layout;
    QGroupBox *_mouse_groupBox;
    QCheckBox *_fen_checkBox;
    QLabel *_width_label;
    QLabel *_period_label;
    QLabel *_freq_label;
    QLabel *_duty_label;
    QLabel *_add_dec_label;
    QGridLayout *_dist_layout;
    QGroupBox *_dist_groupBox;
    QToolButton *_dist_add_btn;
    QGridLayout *_edge_layout;
    QGroupBox *_edge_groupBox;
    QToolButton *_edge_add_btn;
    QPushButton *_sel_btn;

    QGridLayout *_cursor_layout;
    QGroupBox *_cursor_groupBox;
    std::vector<cursor_distance_info>   _cursor_disdance_list;
    std::vector<cursor_edge_info>       _cursor_edge_list;
    std::vector<cursor_opt_info>        _cursor_opt_list;

    QLabel *_channel_label;
    QLabel *_edge_label;
    QLabel *_time_label;
    QLabel *_w_label;
    QLabel *_p_label;
    QLabel *_f_label;
    QLabel *_d_label;
};

} // namespace dock
} // namespace pv

#endif // DSVIEW_PV_MEASUREDOCK_H
