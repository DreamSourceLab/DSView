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

#include <math.h>

#include "measuredock.h"
#include "../sigsession.h"
#include "../view/cursor.h"
#include "../view/view.h"
#include "../view/viewport.h"
#include "../view/timemarker.h"
#include "../view/ruler.h"
#include "../view/logicsignal.h"
#include "../data/signaldata.h"
#include "../data/snapshot.h"
#include "../devicemanager.h"
#include "../device/device.h"
#include "../device/file.h"

#include <QObject>
#include <QPainter>
#include <QRegExpValidator>

#include "libsigrok4DSL/libsigrok.h"

using boost::shared_ptr;

namespace pv {
namespace dock {

using namespace pv::view;

MeasureDock::MeasureDock(QWidget *parent, View &view, SigSession &session) :
    QScrollArea(parent),
    _session(session),
    _view(view)
{

    _widget = new QWidget(this);
    //_widget->setSizePolicy();

    _mouse_groupBox = new QGroupBox(tr("Mouse measurement"), _widget);
    _fen_checkBox = new QCheckBox(tr("Enable floating measurement"), _widget);
    _fen_checkBox->setChecked(true);
    _width_label = new QLabel("#####", _widget);
    _period_label = new QLabel("#####", _widget);
    _freq_label = new QLabel("#####", _widget);
    _duty_label = new QLabel("#####", _widget);

    _mouse_layout = new QGridLayout();
    _mouse_layout->setVerticalSpacing(5);
    _mouse_layout->addWidget(_fen_checkBox, 0, 0, 1, 4);
    _mouse_layout->addWidget(new QLabel(tr("W: "), _widget), 1, 0);
    _mouse_layout->addWidget(_width_label, 1, 1);
    _mouse_layout->addWidget(new QLabel(tr("P: "), _widget), 1, 2);
    _mouse_layout->addWidget(_period_label, 1, 3);
    _mouse_layout->addWidget(new QLabel(tr("F: "), _widget), 2, 2);
    _mouse_layout->addWidget(_freq_label, 2, 3);
    _mouse_layout->addWidget(new QLabel(tr("D: "), _widget), 2, 0);
    _mouse_layout->addWidget(_duty_label, 2, 1);
    _mouse_layout->addWidget(new QLabel(_widget), 0, 4);
    _mouse_layout->addWidget(new QLabel(_widget), 1, 4);
    _mouse_layout->addWidget(new QLabel(_widget), 2, 4);
    _mouse_layout->setColumnStretch(5, 1);
    _mouse_groupBox->setLayout(_mouse_layout);


    _cursor_groupBox = new QGroupBox(tr("Cursor measurement"), _widget);
    _t1_comboBox = new QComboBox(_widget);
    _t2_comboBox = new QComboBox(_widget);
    _t3_comboBox = new QComboBox(_widget);
    _delta_label_t1t2 = new QLabel("##########/##########", _widget);
    _delta_label_t2t3 = new QLabel("##########/##########", _widget);
    _delta_label_t1t3 = new QLabel("##########/##########", _widget);
    _t1_last_index = 0;
    _t2_last_index = 0;
    _t3_last_index = 0;
    _t1_comboBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    _t2_comboBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    _t3_comboBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);

    _cursor_layout = new QGridLayout(_widget);
    _cursor_layout->setVerticalSpacing(5);
    _cursor_layout->addWidget(new QLabel(tr("T1: "), _widget), 0, 0);
    _cursor_layout->addWidget(_t1_comboBox, 0, 1);
    _cursor_layout->addWidget(new QLabel(tr("T2: "), _widget), 1, 0);
    _cursor_layout->addWidget(_t2_comboBox, 1, 1);
    _cursor_layout->addWidget(new QLabel(tr("T3: "), _widget), 2, 0);
    _cursor_layout->addWidget(_t3_comboBox, 2, 1);

    _cursor_layout->addWidget(new QLabel(tr("Time/Samples"), _widget), 3, 1, 1, 2);
    _cursor_layout->addWidget(new QLabel(tr("|T2 - T1|: "), _widget), 4, 0);
    _cursor_layout->addWidget(_delta_label_t1t2, 4, 1, 1, 2);

    _cursor_layout->addWidget(new QLabel(tr("|T3 - T2|: "), _widget), 5, 0);
    _cursor_layout->addWidget(_delta_label_t2t3, 5, 1, 1, 2);

    _cursor_layout->addWidget(new QLabel(tr("|T3 - T1|: "), _widget), 6, 0);
    _cursor_layout->addWidget(_delta_label_t1t3, 6, 1, 1, 2);

    _cursor_layout->addWidget(new QLabel(_widget), 7, 0);
    _cursor_layout->addWidget(new QLabel(tr("Cursors"), _widget), 8, 0);
    _cursor_layout->addWidget(new QLabel(tr("Time/Samples"), _widget), 8, 1, 1, 2);

    _cursor_layout->addWidget(new QLabel(_widget), 0, 2);
    _cursor_layout->addWidget(new QLabel(_widget), 1, 2);
    _cursor_layout->addWidget(new QLabel(_widget), 2, 2);
    _cursor_layout->setColumnStretch(2, 1);

    _cursor_groupBox->setLayout(_cursor_layout);

    QVBoxLayout *layout = new QVBoxLayout(_widget);
    layout->addWidget(_mouse_groupBox);
    layout->addWidget(_cursor_groupBox);
    layout->addStretch(1);
    _widget->setLayout(layout);

    connect(_t1_comboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(delta_update()));
    connect(_t2_comboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(delta_update()));
    connect(_t3_comboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(delta_update()));

    connect(_fen_checkBox, SIGNAL(stateChanged(int)), &_view, SLOT(set_measure_en(int)));
    connect(&_view, SIGNAL(measure_updated()), this, SLOT(measure_updated()));

    this->setWidget(_widget);
    _widget->setGeometry(0, 0, sizeHint().width(), 2000);
    _widget->setObjectName("measureWidget");
}

MeasureDock::~MeasureDock()
{
}

void MeasureDock::paintEvent(QPaintEvent *)
{
//    QStyleOption opt;
//    opt.init(this);
//    QPainter p(this);
//    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

void MeasureDock::cursor_update()
{
    using namespace pv::data;

    int index = 1;

    disconnect(_t1_comboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(delta_update()));
    disconnect(_t2_comboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(delta_update()));
    disconnect(_t3_comboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(delta_update()));
    _t1_comboBox->clear();
    _t2_comboBox->clear();
    _t3_comboBox->clear();

    if (!_cursor_pushButton_list.empty()) {
        for (QVector<QPushButton *>::Iterator i = _cursor_pushButton_list.begin();
             i != _cursor_pushButton_list.end(); i++)
            delete (*i);
        for (QVector<QLabel *>::Iterator i = _curpos_label_list.begin();
             i != _curpos_label_list.end(); i++)
            delete (*i);
        for (QVector<QLabel *>::Iterator i = _space_label_list.begin();
             i != _space_label_list.end(); i++)
            delete (*i);

        _cursor_pushButton_list.clear();
        _curpos_label_list.clear();
        _space_label_list.clear();
    }

    for(std::list<Cursor*>::iterator i = _view.get_cursorList().begin();
        i != _view.get_cursorList().end(); i++) {
        QString curCursor = tr("Cursor ")+QString::number(index);
        _t1_comboBox->addItem(curCursor);
        _t2_comboBox->addItem(curCursor);
        _t3_comboBox->addItem(curCursor);

        QPushButton *_cursor_pushButton = new QPushButton(curCursor, _widget);
        QString _cur_text = _view.get_cm_time(index - 1) + "/" + QString::number(_view.get_cursor_samples(index - 1));
        QLabel *_curpos_label = new QLabel(_cur_text, _widget);
        QLabel *_space_label = new QLabel(_widget);
        _cursor_pushButton_list.push_back(_cursor_pushButton);
        _curpos_label_list.push_back(_curpos_label);
        _space_label_list.push_back(_space_label);

        _cursor_layout->addWidget(_cursor_pushButton, 8 + index, 0);
        _cursor_layout->addWidget(_curpos_label, 8 + index, 1, 1, 2);

        connect(_cursor_pushButton, SIGNAL(clicked()), this, SLOT(goto_cursor()));

        index++;
    }
    _t1_comboBox->setMinimumWidth(_t1_comboBox->sizeHint().width()+30);
    _t2_comboBox->setMinimumWidth(_t2_comboBox->sizeHint().width()+30);
    _t3_comboBox->setMinimumWidth(_t3_comboBox->sizeHint().width()+30);

    if (_t1_last_index < _t1_comboBox->count())
        _t1_comboBox->setCurrentIndex(_t1_last_index);
    if (_t2_last_index < _t2_comboBox->count())
        _t2_comboBox->setCurrentIndex(_t2_last_index);
    if (_t3_last_index < _t3_comboBox->count())
        _t3_comboBox->setCurrentIndex(_t3_last_index);

    connect(_t1_comboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(delta_update()));
    connect(_t2_comboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(delta_update()));
    connect(_t3_comboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(delta_update()));

    delta_update();
    update();
}

void MeasureDock::measure_updated()
{
    _width_label->setText(_view.get_measure("width"));
    _period_label->setText(_view.get_measure("period"));
    _freq_label->setText(_view.get_measure("frequency"));
    _duty_label->setText(_view.get_measure("duty"));
}

void MeasureDock::cursor_moved()
{
    //TimeMarker* grabbed_marker = _view.get_ruler()->get_grabbed_cursor();
    if (_view.cursors_shown()) {
        int index = 0;
        for(std::list<Cursor*>::iterator i = _view.get_cursorList().begin();
            i != _view.get_cursorList().end(); i++) {
            QString _cur_text = _view.get_cm_time(index) + "/" + QString::number(_view.get_cursor_samples(index));
            _curpos_label_list.at(index)->setText(_cur_text);
            //_curvalue_label_list.at(index)->setText(_view.get_cm_value(index));
            index++;
        }
    }
    delta_update();
}

void MeasureDock::delta_update()
{
    QString delta_text;
    _t1_last_index = std::max(_t1_comboBox->currentIndex(), 0);
    _t2_last_index = std::max(_t2_comboBox->currentIndex(), 0);
    _t3_last_index = std::max(_t3_comboBox->currentIndex(), 0);
    if (_t1_comboBox->count() != 0 && _t2_comboBox->count() != 0) {
        uint64_t delta = abs(_view.get_cursor_samples(_t1_last_index) -
                             _view.get_cursor_samples(_t2_last_index));
        delta_text = _view.get_cm_delta(_t1_last_index, _t2_last_index) +
                             "/" + QString::number(delta);
        _delta_label_t1t2->setText(delta_text);
    }

    if (_t2_comboBox->count() != 0 && _t2_comboBox->count() != 0) {
        uint64_t delta = abs(_view.get_cursor_samples(_t2_last_index) -
                             _view.get_cursor_samples(_t3_last_index));
        delta_text = _view.get_cm_delta(_t2_last_index, _t3_last_index) +
                             "/" + QString::number(delta);
        _delta_label_t2t3->setText(delta_text);
    }

    if (_t1_comboBox->count() != 0 && _t3_comboBox->count() != 0) {
        uint64_t delta = abs(_view.get_cursor_samples(_t1_last_index) -
                             _view.get_cursor_samples(_t3_last_index));
        delta_text = _view.get_cm_delta(_t1_last_index, _t3_last_index) +
                             "/" + QString::number(delta);
        _delta_label_t1t3->setText(delta_text);
    }
}

void MeasureDock::goto_cursor()
{
    int index = 0;

    for (QVector<QPushButton *>::Iterator i = _cursor_pushButton_list.begin();
         i != _cursor_pushButton_list.end(); i++) {
        QPushButton *button = qobject_cast<QPushButton *>(sender());
        if ((*i) == button) {
            _view.set_cursor_middle(index);
            break;
        }
        index++;
    }
}

} // namespace dock
} // namespace pv
