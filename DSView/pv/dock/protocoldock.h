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


#ifndef DSVIEW_PV_PROTOCOLDOCK_H
#define DSVIEW_PV_PROTOCOLDOCK_H

#include <libsigrokdecode/libsigrokdecode.h>

#include <QDockWidget>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QVector>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>

#include <vector>

#include <libsigrok4DSL/libsigrok.h>

namespace pv {

class SigSession;

namespace dock {

class ProtocolDock : public QScrollArea
{
    Q_OBJECT

public:
    ProtocolDock(QWidget *parent, SigSession &session);
    ~ProtocolDock();

    void paintEvent(QPaintEvent *);

    void del_all_protocol();

signals:

private slots:
    void add_protocol();
    void rst_protocol();
    void del_protocol();

private:
    static int decoder_name_cmp(const void *a, const void *b);

private:
    SigSession &_session;

    QWidget *_widget;

    QPushButton *_add_button;
    QPushButton *_del_all_button;
    QComboBox *_protocol_combobox;
    QVector <QPushButton *> _del_button_list;
    QVector <QPushButton *> _set_button_list;
    QVector <QLabel *> _protocol_label_list;
    QVector <int > _protocol_index_list;
    QVector <QHBoxLayout *> _hori_layout_list;
    QVBoxLayout *_layout;
};

} // namespace dock
} // namespace pv

#endif // DSVIEW_PV_PROTOCOLDOCK_H
