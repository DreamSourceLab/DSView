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
#include "../dialogs/dsdialog.h"
#include "../dialogs/dsmessagebox.h"

#include <QObject>
#include <QPainter>
#include <QRegExpValidator>
#include <QMessageBox>

using namespace boost;

namespace pv {
namespace dock {

using namespace pv::view;

MeasureDock::MeasureDock(QWidget *parent, View &view, SigSession &session) :
    QScrollArea(parent),
    _session(session),
    _view(view),
    _icon_add(":/icons/add.png"),
    _icon_add_dis(":/icons/add_dis.png"),
    _icon_del(":/icons/del.png"),
    _icon_del_dis(":/icons/del_dis.png")
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
    _mouse_layout->addWidget(_fen_checkBox, 0, 0, 1, 6);
    _mouse_layout->addWidget(new QLabel(tr("W: "), _widget), 1, 0);
    _mouse_layout->addWidget(_width_label, 1, 1);
    _mouse_layout->addWidget(new QLabel(tr("P: "), _widget), 1, 4);
    _mouse_layout->addWidget(_period_label, 1, 5);
    _mouse_layout->addWidget(new QLabel(tr("F: "), _widget), 2, 4);
    _mouse_layout->addWidget(_freq_label, 2, 5);
    _mouse_layout->addWidget(new QLabel(tr("D: "), _widget), 2, 0);
    _mouse_layout->addWidget(_duty_label, 2, 1);
    _mouse_layout->addWidget(new QLabel(_widget), 0, 6);
    _mouse_layout->addWidget(new QLabel(_widget), 1, 6);
    _mouse_layout->addWidget(new QLabel(_widget), 2, 6);
    _mouse_layout->setColumnStretch(5, 1);
    _mouse_groupBox->setLayout(_mouse_layout);

    /* cursor distance group */
    _dist_groupBox = new QGroupBox(tr("Cursor Distance"), _widget);
    _dist_groupBox->setMinimumWidth(300);
    _dist_add_btn = new QToolButton(_widget);
    _dist_add_btn->setIcon(_icon_add);
    connect(_dist_add_btn, SIGNAL(clicked()), this, SLOT(add_dist_measure()));

    _dist_layout = new QGridLayout(_widget);
    _dist_layout->setVerticalSpacing(5);
    _dist_layout->addWidget(_dist_add_btn, 0, 0);
    _dist_layout->addWidget(new QLabel(_widget), 0, 1, 1, 3);
    _dist_layout->addWidget(new QLabel(tr("Time/Samples"), _widget), 0, 4);
    _dist_layout->addWidget(new QLabel(_widget), 0, 5, 1, 2);
    _dist_layout->setColumnStretch(1, 50);
    _dist_layout->setColumnStretch(6, 100);
    add_dist_measure();
    _dist_groupBox->setLayout(_dist_layout);

    /* cursor edges group */
    _edge_groupBox = new QGroupBox(tr("Edges"), _widget);
    _edge_groupBox->setMinimumWidth(300);
    _edge_add_btn = new QToolButton(_widget);
    _edge_add_btn->setIcon(_icon_add);
    connect(_edge_add_btn, SIGNAL(clicked()), this, SLOT(add_edge_measure()));

    _edge_layout = new QGridLayout(_widget);
    _edge_layout->setVerticalSpacing(5);
    _edge_layout->addWidget(_edge_add_btn, 0, 0);
    _edge_layout->addWidget(new QLabel(_widget), 0, 1, 1, 4);
    _edge_layout->addWidget(new QLabel(tr("Channel"), _widget), 0, 5);
    _edge_layout->addWidget(new QLabel(tr("Rising/Falling/Edges"), _widget), 0, 6);
    _edge_layout->setColumnStretch(1, 50);
    //_edge_layout->setColumnStretch(6, 100);
    //add_edge_measure();
    _edge_groupBox->setLayout(_edge_layout);

    /* cursors group */
    _cursor_groupBox = new QGroupBox(tr("Cursors"), _widget);
    _cursor_layout = new QGridLayout(_widget);
    _cursor_layout->addWidget(new QLabel(tr("Time/Samples"), _widget), 0, 2);
    _cursor_layout->addWidget(new QLabel(_widget), 0, 3);
    _cursor_layout->setColumnStretch(3, 1);

    _cursor_groupBox->setLayout(_cursor_layout);

    QVBoxLayout *layout = new QVBoxLayout(_widget);
    layout->addWidget(_mouse_groupBox);
    layout->addWidget(_dist_groupBox);
    layout->addWidget(_edge_groupBox);
    layout->addWidget(_cursor_groupBox);
    layout->addStretch(1);
    _widget->setLayout(layout);

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

void MeasureDock::refresh()
{

}

void MeasureDock::reload()
{
    for (QVector <QComboBox *>::const_iterator i = _edge_ch_cmb_vec.begin();
         i != _edge_ch_cmb_vec.end(); i++) {
        update_probe_selector(*i);
    }
    reCalc();
}

void MeasureDock::cursor_update()
{
    using namespace pv::data;

    if (!_cursor_pushButton_list.empty()) {
        for (QVector <QToolButton *>::const_iterator i = _cursor_del_btn_vec.begin();
             i != _cursor_del_btn_vec.end(); i++)
            delete (*i);
        for (QVector<QPushButton *>::Iterator i = _cursor_pushButton_list.begin();
             i != _cursor_pushButton_list.end(); i++)
            delete (*i);
        for (QVector<QLabel *>::Iterator i = _curpos_label_list.begin();
             i != _curpos_label_list.end(); i++)
            delete (*i);

        _cursor_del_btn_vec.clear();
        _cursor_pushButton_list.clear();
        _curpos_label_list.clear();
    }

    update_dist();
    update_edge();

    int index = 1;
    for(std::list<Cursor*>::iterator i = _view.get_cursorList().begin();
        i != _view.get_cursorList().end(); i++) {
        QString curCursor = QString::number(index);

        QToolButton *del_btn = new QToolButton(_widget);
        del_btn->setIcon(QIcon::fromTheme("measure",
                                 QIcon(":/icons/del.png")));
        del_btn->setCheckable(true);
        QPushButton *_cursor_pushButton = new QPushButton(curCursor, _widget);
        set_cursor_btn_color(_cursor_pushButton);
        QString _cur_text = _view.get_cm_time(index - 1) + "/" + QString::number(_view.get_cursor_samples(index - 1));
        QLabel *_curpos_label = new QLabel(_cur_text, _widget);
        _cursor_del_btn_vec.push_back(del_btn);
        _cursor_pushButton_list.push_back(_cursor_pushButton);
        _curpos_label_list.push_back(_curpos_label);

        _cursor_layout->addWidget(del_btn, 1+index, 0);
        _cursor_layout->addWidget(_cursor_pushButton, 1 + index, 1);
        _cursor_layout->addWidget(_curpos_label, 1 + index, 2);

        connect(del_btn, SIGNAL(clicked()), this, SLOT(del_cursor()));
        connect(_cursor_pushButton, SIGNAL(clicked()), this, SLOT(goto_cursor()));

        index++;
    }

    update();
}

void MeasureDock::measure_updated()
{
    _width_label->setText(_view.get_measure("width"));
    _period_label->setText(_view.get_measure("period"));
    _freq_label->setText(_view.get_measure("frequency"));
    _duty_label->setText(_view.get_measure("duty"));
}

void MeasureDock::cursor_moving()
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

    update_dist();
}

void MeasureDock::reCalc()
{
    cursor_update();
    update_dist();
    update_edge();
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

void MeasureDock::add_dist_measure()
{
    if (_dist_row_widget_vec.size() > Max_Measure_Limits)
        return;

    QWidget *row_widget = new QWidget(_widget);
    row_widget->setContentsMargins(0,0,0,0);
    QHBoxLayout *row_layout = new QHBoxLayout(row_widget);
    row_layout->setContentsMargins(0,0,0,0);
    row_layout->setSpacing(0);
    row_widget->setLayout(row_layout);
    _dist_row_widget_vec.push_back(row_widget);

    QToolButton *del_btn = new QToolButton(row_widget);
    del_btn->setIcon(QIcon::fromTheme("measure",
                             QIcon(":/icons/del.png")));
    del_btn->setCheckable(true);
    QPushButton *s_btn = new QPushButton(tr(" "), row_widget);
    s_btn->setObjectName("dist");
    QPushButton *e_btn = new QPushButton(tr(" "), row_widget);
    e_btn->setObjectName("dist");
    QLabel *r_label = new QLabel(row_widget);
    QLabel *g_label = new QLabel(tr("-"), row_widget);
    g_label->setContentsMargins(0,0,0,0);
    _dist_del_btn_vec.push_back(del_btn);
    _dist_s_btn_vec.push_back(s_btn);
    _dist_e_btn_vec.push_back(e_btn);
    _dist_r_label_vec.push_back(r_label);

    connect(del_btn, SIGNAL(clicked()), this, SLOT(del_dist_measure()));
    connect(s_btn, SIGNAL(clicked()), this, SLOT(show_all_coursor()));
    connect(e_btn, SIGNAL(clicked()), this, SLOT(show_all_coursor()));

    row_layout->addWidget(del_btn);
    row_layout->addSpacing(5);
    row_layout->addWidget(s_btn);
    row_layout->addWidget(g_label);
    row_layout->addWidget(e_btn);
    row_layout->addSpacing(5);
    row_layout->addWidget(r_label, 100);

    _dist_layout->addWidget(row_widget, _dist_row_widget_vec.size(), 0, 1, 7);

}

void MeasureDock::del_dist_measure()
{
    int del_index = 0;
    for (QVector <QToolButton *>::const_iterator i = _dist_del_btn_vec.begin();
         i != _dist_del_btn_vec.end(); i++) {
        if ((*i)->isChecked()) {
            _dist_layout->removeWidget(_dist_row_widget_vec.at(del_index));

            delete _dist_del_btn_vec.at(del_index);
            delete _dist_s_btn_vec.at(del_index);
            delete _dist_e_btn_vec.at(del_index);
            delete _dist_r_label_vec.at(del_index);
            delete _dist_row_widget_vec.at(del_index);

            _dist_del_btn_vec.remove(del_index);
            _dist_s_btn_vec.remove(del_index);
            _dist_e_btn_vec.remove(del_index);
            _dist_r_label_vec.remove(del_index);
            _dist_row_widget_vec.remove(del_index);

            break;
        }
        del_index++;
    }
}

void MeasureDock::add_edge_measure()
{
    if (_edge_row_widget_vec.size() > Max_Measure_Limits)
        return;

    QWidget *row_widget = new QWidget(_widget);
    row_widget->setContentsMargins(0,0,0,0);
    QHBoxLayout *row_layout = new QHBoxLayout(row_widget);
    row_layout->setContentsMargins(0,0,0,0);
    row_layout->setSpacing(0);
    row_widget->setLayout(row_layout);
    _edge_row_widget_vec.push_back(row_widget);

    QToolButton *del_btn = new QToolButton(row_widget);
    del_btn->setIcon(QIcon::fromTheme("measure",
                             QIcon(":/icons/del.png")));
    del_btn->setCheckable(true);
    QPushButton *s_btn = new QPushButton(tr(" "), row_widget);
    s_btn->setObjectName("edge");
    QPushButton *e_btn = new QPushButton(tr(" "), row_widget);
    e_btn->setObjectName("edge");
    QLabel *r_label = new QLabel(row_widget);
    QLabel *g_label = new QLabel(tr("-"), row_widget);
    g_label->setContentsMargins(0,0,0,0);
    QLabel *a_label = new QLabel(tr("@"), row_widget);
    a_label->setContentsMargins(0,0,0,0);
    QComboBox *ch_cmb = create_probe_selector(row_widget);
    _edge_del_btn_vec.push_back(del_btn);
    _edge_s_btn_vec.push_back(s_btn);
    _edge_e_btn_vec.push_back(e_btn);
    _edge_ch_cmb_vec.push_back(ch_cmb);
    _edge_r_label_vec.push_back(r_label);

    connect(del_btn, SIGNAL(clicked()), this, SLOT(del_edge_measure()));
    connect(s_btn, SIGNAL(clicked()), this, SLOT(show_all_coursor()));
    connect(e_btn, SIGNAL(clicked()), this, SLOT(show_all_coursor()));
    connect(ch_cmb, SIGNAL(currentIndexChanged(int)), this, SLOT(update_edge()));

    row_layout->addWidget(del_btn);
    row_layout->addSpacing(5);
    row_layout->addWidget(s_btn);
    row_layout->addWidget(g_label);
    row_layout->addWidget(e_btn);
    row_layout->addWidget(a_label);
    row_layout->addWidget(ch_cmb);
    row_layout->addSpacing(5);
    row_layout->addWidget(r_label, 100);

    _edge_layout->addWidget(row_widget, _edge_row_widget_vec.size(), 0, 1, 7);

}

void MeasureDock::del_edge_measure()
{
    int del_index = 0;
    for (QVector <QToolButton *>::const_iterator i = _edge_del_btn_vec.begin();
         i != _edge_del_btn_vec.end(); i++) {
        if ((*i)->isChecked()) {
            _edge_layout->removeWidget(_edge_row_widget_vec.at(del_index));

            delete _edge_del_btn_vec.at(del_index);
            delete _edge_s_btn_vec.at(del_index);
            delete _edge_e_btn_vec.at(del_index);
            delete _edge_r_label_vec.at(del_index);
            delete _edge_ch_cmb_vec.at(del_index);
            delete _edge_row_widget_vec.at(del_index);

            _edge_del_btn_vec.remove(del_index);
            _edge_s_btn_vec.remove(del_index);
            _edge_e_btn_vec.remove(del_index);
            _edge_r_label_vec.remove(del_index);
            _edge_ch_cmb_vec.remove(del_index);
            _edge_row_widget_vec.remove(del_index);

            break;
        }
        del_index++;
    }
}

void MeasureDock::show_all_coursor()
{
    if (_view.get_cursorList().empty()) {
        dialogs::DSMessageBox msg(this);
        msg.mBox()->setText(tr("Information"));
        msg.mBox()->setInformativeText(tr("Please insert cursor before using cursor measure."));
        msg.mBox()->addButton(tr("Ok"), QMessageBox::AcceptRole);
        msg.mBox()->setIcon(QMessageBox::Information);
        msg.exec();

        return;
    }

    _sel_btn = qobject_cast<QPushButton *>(sender());

    //dialogs::DSDialog cursor_dlg(_widget);
    QDialog cursor_dlg(_widget);
    cursor_dlg.setWindowFlags(Qt::FramelessWindowHint | Qt::Popup | Qt::WindowSystemMenuHint |
                              Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint);
    int index = 0;
    QHBoxLayout *hlayout = NULL;
    QVBoxLayout *vlayout = new QVBoxLayout(&cursor_dlg);
    for(std::list<Cursor*>::iterator i = _view.get_cursorList().begin();
        i != _view.get_cursorList().end(); i++) {
        QPushButton *cursor_btn = new QPushButton(QString::number(index+1), &cursor_dlg);
        set_cursor_btn_color(cursor_btn);
        if ((index % 4) == 0) {
            hlayout = new QHBoxLayout(&cursor_dlg);
            vlayout->addLayout(hlayout);
            //cursor_dlg.layout()->addLayout(hlayout);
        }
        hlayout->addWidget(cursor_btn);
        connect(cursor_btn, SIGNAL(clicked()), &cursor_dlg, SLOT(accept()));
        connect(cursor_btn, SIGNAL(clicked()), this, SLOT(set_se_cursor()));
        index++;
    }
    while((index++ % 4) != 0)
        hlayout->addWidget(new QLabel(&cursor_dlg));

    cursor_dlg.setLayout(vlayout);
    QRect sel_btn_rect = _sel_btn->geometry();
    sel_btn_rect.moveTopLeft(_sel_btn->parentWidget()->mapToGlobal(sel_btn_rect.topLeft()));
    cursor_dlg.setGeometry(sel_btn_rect.left(), sel_btn_rect.bottom()+10,
                           cursor_dlg.width(), cursor_dlg.height());
    cursor_dlg.exec();
}

void MeasureDock::set_se_cursor()
{
    QPushButton *sc = qobject_cast<QPushButton *>(sender());
    if (_sel_btn)
        _sel_btn->setText(sc->text());

    set_cursor_btn_color(_sel_btn);

    if (_sel_btn->objectName() == "dist")
        update_dist();
    else if (_sel_btn->objectName() == "edge")
        update_edge();
}

const view::Cursor* MeasureDock::find_cousor(int index)
{
    int cur_index = 1;
    for(std::list<Cursor*>::iterator i = _view.get_cursorList().begin();
        i != _view.get_cursorList().end(); i++) {
        if (cur_index == index) {
            return (*i);
        }
    }

    return NULL;
}

void MeasureDock::update_dist()
{
    int dist_index = 0;
    for (QVector<QPushButton *>::Iterator i = _dist_s_btn_vec.begin();
         i != _dist_s_btn_vec.end(); i++) {
        bool start_ret, end_ret;
        const unsigned int start = (*i)->text().toInt(&start_ret) - 1;
        const unsigned int end = _dist_e_btn_vec[dist_index]->text().toInt(&end_ret) - 1;

        if (start_ret) {
            if (start + 1 > _view.get_cursorList().size()) {
                (*i)->setText(" ");
                set_cursor_btn_color((*i));
                start_ret = false;
            }
        }
        if (end_ret) {
            if (end + 1 > _view.get_cursorList().size()) {
                _dist_e_btn_vec[dist_index]->setText(" ");
                set_cursor_btn_color(_dist_e_btn_vec[dist_index]);
                end_ret = false;
            }
        }

        if (start_ret && end_ret) {
            int64_t delta = _view.get_cursor_samples(start) -
                            _view.get_cursor_samples(end);
            QString delta_text = _view.get_cm_delta(start, end) +
                                 "/" + QString::number(delta);
            if (delta < 0)
                delta_text.replace('+', '-');
            _dist_r_label_vec[dist_index]->setText(delta_text);
        } else {
            _dist_r_label_vec[dist_index]->setText(" ");
        }

        dist_index++;
    }
}

void MeasureDock::update_edge()
{
    int edge_index = 0;
    for (QVector<QPushButton *>::Iterator i = _edge_s_btn_vec.begin();
         i != _edge_s_btn_vec.end(); i++) {
        bool start_ret, end_ret;
        const unsigned int start = (*i)->text().toInt(&start_ret) - 1;
        const unsigned int end = _edge_e_btn_vec[edge_index]->text().toInt(&end_ret) - 1;

        if (start_ret) {
            if (start + 1 > _view.get_cursorList().size()) {
                (*i)->setText(" ");
                set_cursor_btn_color((*i));
                start_ret = false;
            }
        }
        if (end_ret) {
            if (end + 1 > _view.get_cursorList().size()) {
                _edge_e_btn_vec[edge_index]->setText(" ");
                set_cursor_btn_color(_edge_e_btn_vec[edge_index]);
                end_ret = false;
            }
        }

        bool mValid = false;
        if (start_ret && end_ret) {
            uint64_t rising_edges;
            uint64_t falling_edges;
            const std::vector< boost::shared_ptr<Signal> > sigs(_session.get_signals());
            for(size_t i = 0; i < sigs.size(); i++) {
                const boost::shared_ptr<view::Signal> s(sigs[i]);
                boost::shared_ptr<view::LogicSignal> logicSig;
                assert(s);
                if ((logicSig = dynamic_pointer_cast<view::LogicSignal>(s)) &&
                    (logicSig->enabled()) &&
                    (logicSig->get_index() == _edge_ch_cmb_vec[edge_index]->currentText().toInt())){
                    if (logicSig->edges(_view.get_cursor_samples(end), _view.get_cursor_samples(start), rising_edges, falling_edges)) {
                        QString delta_text = QString::number(rising_edges) + "/" +
                                             QString::number(falling_edges) + "/" +
                                             QString::number(rising_edges + falling_edges);
                        _edge_r_label_vec[edge_index]->setText(delta_text);
                        mValid = true;
                        break;
                    }
                }
            }
        }

        if (!mValid)
            _edge_r_label_vec[edge_index]->setText("-/-/-");

        edge_index++;
    }
}

void MeasureDock::set_cursor_btn_color(QPushButton *btn)
{
    bool ret;
    const unsigned int start = btn->text().toInt(&ret) - 1;
    QColor cursor_color = ret ? view::Ruler::CursorColor[start%8] : QColor("#302F2F");
    QString border_width = ret ? "0px" : "1px";
    QString normal = "{background-color:" + cursor_color.name() +
            "; color:black" + "; border-width:" + border_width + ";}";
    QString hover = "{background-color:" + cursor_color.darker().name() +
            "; color:black" + "; border-width:" + border_width + ";}";
    QString style = "QPushButton:hover" + hover +
                    "QPushButton" + normal;
    btn->setStyleSheet(style);
}

QComboBox* MeasureDock::create_probe_selector(QWidget *parent)
{
    QComboBox *selector = new QComboBox(parent);
    update_probe_selector(selector);
    return selector;
}

void MeasureDock::update_probe_selector(QComboBox *selector)
{
    selector->clear();
    const std::vector< boost::shared_ptr<Signal> > sigs(_session.get_signals());
    for(size_t i = 0; i < sigs.size(); i++) {
        const boost::shared_ptr<view::Signal> s(sigs[i]);
        assert(s);

        if (dynamic_pointer_cast<LogicSignal>(s) && s->enabled())
        {
            selector->addItem(QString::number(s->get_index()));
        }
    }
}

void MeasureDock::del_cursor()
{
    int del_index = 0;
    Cursor* cursor = NULL;
    for (QVector <QToolButton *>::const_iterator i = _cursor_del_btn_vec.begin();
         i != _cursor_del_btn_vec.end(); i++) {
        if ((*i)->isChecked()) {
            int cur_index = 0;
            std::list<Cursor*>::iterator ite = _view.get_cursorList().begin();
            while (cur_index++ != del_index)
                ite++;
            cursor = *ite;
            break;
        }
        del_index++;
    }

    if (cursor)
        _view.del_cursor(cursor);
    if (_view.get_cursorList().empty())
        _view.show_cursors(false);

    cursor_update();
    _view.update();
}

} // namespace dock
} // namespace pv
