/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
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


#ifndef DSVIEW_PV_FFTOPTIONS_H
#define DSVIEW_PV_FFTOPTIONS_H

#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QCheckBox>
#include <QComboBox>

#include <boost/shared_ptr.hpp>

#include "../device/devinst.h"

namespace pv {

class SigSession;

namespace dialogs {

class FftOptions : public QDialog
{
    Q_OBJECT

private:


public:
    FftOptions(QWidget *parent, SigSession &session);

protected:
    void accept();
    void reject();

private slots:
    void window_changed(QString str);
    void len_changed(int index);

private:
    SigSession &_session;
    uint64_t _sample_limit;

    QComboBox *_len_combobox;
    QComboBox *_interval_combobox;
    QCheckBox *_en_checkbox;
    QComboBox *_ch_combobox;
    QComboBox *_window_combobox;
    QCheckBox *_dc_checkbox;
    QComboBox *_view_combobox;
    QComboBox *_dbv_combobox;

    QLabel *_hint_label;
    QFormLayout *_flayout;
    QHBoxLayout *_hlayout;
    QVBoxLayout *_layout;
    QDialogButtonBox _button_box;

};

} // namespace dialogs
} // namespace pv

#endif // DSVIEW_PV_FFTOPTIONS_H
