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

#include "searchdock.h"
#include "../sigsession.h"
#include "../view/cursor.h"
#include "../view/view.h"
#include "../view/timemarker.h"
#include "../view/ruler.h"
#include "../dialogs/search.h"
#include "../data/snapshot.h"
#include "../data/logicsnapshot.h"
#include "../device/devinst.h"
#include "../dialogs/dsmessagebox.h"

#include <QObject>
#include <QPainter>
#include <QRegExpValidator>
#include <QRect>
#include <QMouseEvent>
#include <QFuture>
#include <QProgressDialog>
#include <QtConcurrent/QtConcurrent>

#include <stdint.h>
#include <boost/shared_ptr.hpp>

namespace pv {
namespace dock {

using namespace pv::view;
using namespace pv::widgets;

SearchDock::SearchDock(QWidget *parent, View &view, SigSession &session) :
    QWidget(parent),
    _session(session),
    _view(view)
{
    connect(&_pre_button, SIGNAL(clicked()),
        this, SLOT(on_previous()));
    connect(&_nxt_button, SIGNAL(clicked()),
        this, SLOT(on_next()));

    _pre_button.setIcon(QIcon::fromTheme("searchDock",
        QIcon(":/icons/pre.png")));
    _nxt_button.setIcon(QIcon::fromTheme("searchDock",
        QIcon(":/icons/next.png")));

    _search_button = new QPushButton(this);
    _search_button->setIcon(QIcon::fromTheme("searchDock",
                                             QIcon(":/icons/search.png")));
    _search_button->setFixedWidth(_search_button->height());
    _search_button->setDisabled(true);

    QLineEdit *_search_parent = new QLineEdit(this);
    _search_parent->setVisible(false);
    _search_value = new FakeLineEdit(_search_parent);
    _search_value->setPlaceholderText(tr("search"));

    QHBoxLayout *search_layout = new QHBoxLayout();
    search_layout->addWidget(_search_button);
    search_layout->addStretch();
    search_layout->setContentsMargins(0, 0, 0, 0);
    _search_value->setLayout(search_layout);
    _search_value->setTextMargins(_search_button->width(), 0, 0, 0);
    _search_value->setReadOnly(true);

    connect(_search_value, SIGNAL(trigger()), this, SLOT(on_set()));

    QHBoxLayout *layout = new QHBoxLayout();
    layout->addStretch(1);
    layout->addWidget(&_pre_button);
    layout->addWidget(_search_value);
    layout->addWidget(&_nxt_button);
    layout->addStretch(1);

    setLayout(layout);
}

SearchDock::~SearchDock()
{
}

void SearchDock::paintEvent(QPaintEvent *)
{
//    QStyleOption opt;
//    opt.init(this);
//    QPainter p(this);
//    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

void SearchDock::on_previous()
{
    bool ret;
    int64_t last_pos;
    bool last_hit;
    const boost::shared_ptr<data::Snapshot> snapshot(_session.get_snapshot(SR_CHANNEL_LOGIC));
    assert(snapshot);
    const boost::shared_ptr<data::LogicSnapshot> logic_snapshot = boost::dynamic_pointer_cast<data::LogicSnapshot>(snapshot);

    if (!logic_snapshot || logic_snapshot->empty()) {
        dialogs::DSMessageBox msg(this);
        msg.mBox()->setText(tr("Search"));
        msg.mBox()->setInformativeText(tr("No Sample data!"));
        msg.mBox()->setStandardButtons(QMessageBox::Ok);
        msg.mBox()->setIcon(QMessageBox::Warning);
        msg.exec();
        return;
    }

    const int64_t end = logic_snapshot->get_sample_count() - 1;
    last_pos = _view.get_search_pos();
    last_hit = _view.get_search_hit();
    if (last_pos == 0) {
        dialogs::DSMessageBox msg(this);
        msg.mBox()->setText(tr("Search"));
        msg.mBox()->setInformativeText(tr("Search cursor at the start position!"));
        msg.mBox()->setStandardButtons(QMessageBox::Ok);
        msg.mBox()->setIcon(QMessageBox::Warning);
        msg.exec();
        return;
    } else {
        QFuture<void> future;
        future = QtConcurrent::run([&]{
            last_pos -= last_hit;
            ret = logic_snapshot->pattern_search(0, end, false, last_pos, _pattern);
        });
        Qt::WindowFlags flags = Qt::CustomizeWindowHint;
        QProgressDialog dlg(tr("Search Previous..."),
                            tr("Cancel"),0,0,this,flags);
        dlg.setWindowModality(Qt::WindowModal);
        dlg.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowSystemMenuHint |
                           Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint);
        dlg.setCancelButton(NULL);

        QFutureWatcher<void> watcher;
        connect(&watcher,SIGNAL(finished()),&dlg,SLOT(cancel()));
        watcher.setFuture(future);
        dlg.exec();

        if (!ret) {
            dialogs::DSMessageBox msg(this);
            msg.mBox()->setText(tr("Search"));
            msg.mBox()->setInformativeText(tr("Pattern not found!"));
            msg.mBox()->setStandardButtons(QMessageBox::Ok);
            msg.mBox()->setIcon(QMessageBox::Warning);
            msg.exec();
            return;
        } else {
            _view.set_search_pos(last_pos, true);
        }
    }
}

void SearchDock::on_next()
{
    bool ret;
    int64_t last_pos;
    const boost::shared_ptr<data::Snapshot> snapshot(_session.get_snapshot(SR_CHANNEL_LOGIC));
    assert(snapshot);
    const boost::shared_ptr<data::LogicSnapshot> logic_snapshot = boost::dynamic_pointer_cast<data::LogicSnapshot>(snapshot);

    if (!logic_snapshot || logic_snapshot->empty()) {
        dialogs::DSMessageBox msg(this);
        msg.mBox()->setText(tr("Search"));
        msg.mBox()->setInformativeText(tr("No Sample data!"));
        msg.mBox()->setStandardButtons(QMessageBox::Ok);
        msg.mBox()->setIcon(QMessageBox::Warning);
        msg.exec();
        return;
    }

    const int64_t end = logic_snapshot->get_sample_count() - 1;
    last_pos = _view.get_search_pos() + _view.get_search_hit();
    if (last_pos >= end) {
        dialogs::DSMessageBox msg(this);
        msg.mBox()->setText(tr("Search"));
        msg.mBox()->setInformativeText(tr("Search cursor at the end position!"));
        msg.mBox()->setStandardButtons(QMessageBox::Ok);
        msg.mBox()->setIcon(QMessageBox::Warning);
        msg.exec();
        return;
    } else {
        QFuture<void> future;
        future = QtConcurrent::run([&]{
            ret = logic_snapshot->pattern_search(0, end, true, last_pos, _pattern);
        });
        Qt::WindowFlags flags = Qt::CustomizeWindowHint;
        QProgressDialog dlg(tr("Search Next..."),
                            tr("Cancel"),0,0,this,flags);
        dlg.setWindowModality(Qt::WindowModal);
        dlg.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowSystemMenuHint |
                           Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint);
        dlg.setCancelButton(NULL);

        QFutureWatcher<void> watcher;
        connect(&watcher,SIGNAL(finished()),&dlg,SLOT(cancel()));
        watcher.setFuture(future);
        dlg.exec();

        if (!ret) {
            dialogs::DSMessageBox msg(this);
            msg.mBox()->setText(tr("Search"));
            msg.mBox()->setInformativeText(tr("Pattern not found!"));
            msg.mBox()->setStandardButtons(QMessageBox::Ok);
            msg.mBox()->setIcon(QMessageBox::Warning);
            msg.exec();
            return;
        } else {
            _view.set_search_pos(last_pos, true);
        }
    }
}

void SearchDock::on_set()
{
    dialogs::Search dlg(this, _session, _pattern);
    if (dlg.exec()) {
        std::map<uint16_t, QString> new_pattern = dlg.get_pattern();

        QString search_label;
        for (auto& iter:new_pattern) {
            iter.second.remove(QChar(' '), Qt::CaseInsensitive);
            iter.second = iter.second.toUpper();
            search_label.push_back(iter.second);
        }

        _search_value->setText(search_label);
        QFontMetrics fm = this->fontMetrics();
        _search_value->setFixedWidth(fm.width(search_label)+_search_button->width()+20);

        if (new_pattern != _pattern) {
            _view.set_search_pos(_view.get_search_pos(), false);
            _pattern = new_pattern;
        }
    }
}

} // namespace dock
} // namespace pv
