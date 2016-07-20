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


#ifndef DSVIEW_PV_SEARCHDOCK_H
#define DSVIEW_PV_SEARCHDOCK_H

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

#include <vector>

#include <libsigrok4DSL/libsigrok.h>

#include "../widgets/fakelineedit.h"

namespace pv {

class SigSession;

namespace view {
    class View;
}

namespace widgets {
    class FakeLineEdit;
}

namespace dock {

class SearchDock : public QWidget
{
    Q_OBJECT

public:
    SearchDock(QWidget *parent, pv::view::View &view, SigSession &session);
    ~SearchDock();

    void paintEvent(QPaintEvent *);

signals:
    void search_previous(QString);
    void search_next(QString);

public slots:
    void on_previous();
    void on_next();
    void on_set();
private:
    bool search_value(const uint8_t* data, int unit_size, uint64_t length,
                     uint64_t& pos, bool left, QString value);

private:
    SigSession &_session;
    view::View &_view;
    QString _pattern;

    QPushButton _pre_button;
    QPushButton _nxt_button;
    widgets::FakeLineEdit* _search_value;
    QPushButton *_search_button;
};

} // namespace dock
} // namespace pv

#endif // DSVIEW_PV_SEARCHDOCK_H
