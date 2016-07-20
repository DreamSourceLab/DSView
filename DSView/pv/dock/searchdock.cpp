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
    _pattern = "X X X X X X X X X X X X X X X X";

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
    uint64_t last_pos;
    uint8_t *data;
    int unit_size;
    uint64_t length;
    QString value = _search_value->text();
    search_previous(value);

    last_pos = _view.get_search_pos();
    if (last_pos == 0) {
        dialogs::DSMessageBox msg(this);
        msg.mBox()->setText(tr("Search"));
        msg.mBox()->setInformativeText(tr("Search cursor at the start position!"));
        msg.mBox()->setStandardButtons(QMessageBox::Ok);
        msg.mBox()->setIcon(QMessageBox::Warning);
        msg.exec();
        return;
    } else {
        data = (uint8_t*)_session.get_buf(unit_size, length);
        if (data == NULL) {
            dialogs::DSMessageBox msg(this);
            msg.mBox()->setText(tr("Search"));
            msg.mBox()->setInformativeText(tr("No Sample data!"));
            msg.mBox()->setStandardButtons(QMessageBox::Ok);
            msg.mBox()->setIcon(QMessageBox::Warning);
            msg.exec();
            return;
        } else {
            QFuture<void> future;
            future = QtConcurrent::run([&]{
                ret = search_value(data, unit_size, length, last_pos, 1, value);
            });
            Qt::WindowFlags flags = Qt::CustomizeWindowHint;
            QProgressDialog dlg(tr("Search Previous..."),
                                tr("Cancel"),0,0,this,flags);
            dlg.setWindowModality(Qt::WindowModal);
            dlg.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
            dlg.setCancelButton(NULL);

            QFutureWatcher<void> watcher;
            connect(&watcher,SIGNAL(finished()),&dlg,SLOT(cancel()));
            watcher.setFuture(future);
            dlg.exec();

            if (!ret) {
                dialogs::DSMessageBox msg(this);
                msg.mBox()->setText(tr("Search"));
                msg.mBox()->setInformativeText(tr("Pattern ") + value + tr(" not found!"));
                msg.mBox()->setStandardButtons(QMessageBox::Ok);
                msg.mBox()->setIcon(QMessageBox::Warning);
                msg.exec();
                return;
            } else {
                _view.set_search_pos(last_pos);
            }
        }
    }
}

void SearchDock::on_next()
{
    bool ret;
    uint64_t last_pos;
    int unit_size;
    uint64_t length;
    uint8_t *data = (uint8_t*)_session.get_buf(unit_size, length);
    QString value = _search_value->text();
    search_previous(value);

    last_pos = _view.get_search_pos();
    if (last_pos == length - 1) {
        dialogs::DSMessageBox msg(this);
        msg.mBox()->setText(tr("Search"));
        msg.mBox()->setInformativeText(tr("Search cursor at the end position!"));
        msg.mBox()->setStandardButtons(QMessageBox::Ok);
        msg.mBox()->setIcon(QMessageBox::Warning);
        msg.exec();
        return;
    } else {
        if (data == NULL) {
            dialogs::DSMessageBox msg(this);
            msg.mBox()->setText(tr("Search"));
            msg.mBox()->setInformativeText(tr("No Sample data!"));
            msg.mBox()->setStandardButtons(QMessageBox::Ok);
            msg.mBox()->setIcon(QMessageBox::Warning);
            msg.exec();
            return;
        } else {
            QFuture<void> future;
            future = QtConcurrent::run([&]{
                ret = search_value(data, unit_size, length, last_pos, 0, value);
            });
            Qt::WindowFlags flags = Qt::CustomizeWindowHint;
            QProgressDialog dlg(tr("Search Next..."),
                                tr("Cancel"),0,0,this,flags);
            dlg.setWindowModality(Qt::WindowModal);
            dlg.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
            dlg.setCancelButton(NULL);

            QFutureWatcher<void> watcher;
            connect(&watcher,SIGNAL(finished()),&dlg,SLOT(cancel()));
            watcher.setFuture(future);
            dlg.exec();

            if (!ret) {
                dialogs::DSMessageBox msg(this);
                msg.mBox()->setText(tr("Search"));
                msg.mBox()->setInformativeText(tr("Pattern ") + value + tr(" not found!"));
                msg.mBox()->setStandardButtons(QMessageBox::Ok);
                msg.mBox()->setIcon(QMessageBox::Warning);
                msg.exec();
                return;
            } else {
                _view.set_search_pos(last_pos);
            }
        }
    }
}

void SearchDock::on_set()
{
    dialogs::Search dlg(this, _session.get_device(), _pattern);
    if (dlg.exec()) {
        _pattern = dlg.get_pattern();
        _pattern.remove(QChar(' '), Qt::CaseInsensitive);
        _pattern = _pattern.toUpper();
        _search_value->setText(_pattern);

        QFontMetrics fm = this->fontMetrics();
        _search_value->setFixedWidth(fm.width(_pattern)+_search_button->width()+20);
    }
}

bool SearchDock::search_value(const uint8_t *data, int unit_size, uint64_t length, uint64_t& pos, bool left, QString value)
{
    QByteArray pattern = value.toUtf8();
    int i = 0;
    uint64_t match_pos = left ? pos - 1 : pos + 1;
    bool part_match = false;
    int match_bits = unit_size * 8 - 1;
    bool unmatch = false;

    while(i <= match_bits) {
        unmatch = false;
        uint64_t pattern_mask = 1ULL << i;

        if (pattern[match_bits - i] == 'X') {
            part_match = true;
        } else if (pattern[match_bits - i] == '0') {
            //while((match_pos >= 0 && left) || (match_pos < length && !left)) {
            while(left || (match_pos < length && !left)) {
                if (0 == ((*(uint64_t *)(data + match_pos * unit_size) & pattern_mask) != 0)) {
                    part_match = true;
                    break;
                } else if ((match_pos == 0 && left) || (match_pos == length - 1 && !left)) {
                    unmatch = true;
                    part_match = false;
                    break;
                } else if (part_match) {
                    unmatch = true;
                    match_pos = left ? match_pos - 1 : match_pos + 1;
                    i = 0;
                    break;
                } else if (!part_match) {
                    match_pos = left ? match_pos - 1 : match_pos + 1;
                }
            }
        } else if (pattern[match_bits - i] == '1') {
            //while((match_pos >= 0 && left) || (match_pos < length && !left)) {
            while(left || (match_pos < length && !left)) {
                if (1 == ((*(uint64_t *)(data + match_pos * unit_size) & pattern_mask) != 0)) {
                    part_match = true;
                    break;
                } else if ((match_pos == 0 && left) || (match_pos == length - 1 && !left)) {
                    unmatch = true;
                    part_match = false;
                    break;
                } else if (part_match) {
                    unmatch = true;
                    match_pos = left ? match_pos - 1 : match_pos + 1;
                    i = 0;
                    break;
                }  else if (!part_match) {
                    match_pos = left ? match_pos - 1 : match_pos + 1;
                }
            }
        }else if (pattern[match_bits - i] == 'R') {
            while((match_pos > 0 && left) || (match_pos < length && !left)) {
                if (1 == ((*(uint64_t *)(data + match_pos * unit_size) & pattern_mask) != 0) &&
                    0 == ((*(uint64_t *)(data + (match_pos - 1) * unit_size) & pattern_mask) != 0)) {
                    part_match = true;
                    break;
                } else if ((match_pos == 1 && left) || (match_pos == length - 1 && !left)) {
                    unmatch = true;
                    part_match = false;
                    break;
                } else if (part_match) {
                    unmatch = true;
                    match_pos = left ? match_pos - 1 : match_pos + 1;
                    i = 0;
                    break;
                }  else if (!part_match) {
                    match_pos = left ? match_pos - 1 : match_pos + 1;
                }
            }
        } else if (pattern[match_bits - i] == 'F') {
            while((match_pos > 0 && left) || (match_pos < length && !left)) {
                if (0 == ((*(uint64_t *)(data + match_pos * unit_size) & pattern_mask) != 0) &&
                    1 == ((*(uint64_t *)(data + (match_pos - 1) * unit_size) & pattern_mask) != 0)) {
                    part_match = true;
                    break;
                } else if ((match_pos == 1 && left) || (match_pos == length - 1 && !left)) {
                    unmatch = true;
                    part_match = false;
                    break;
                } else if (part_match) {
                    unmatch = true;
                    match_pos = left ? match_pos - 1 : match_pos + 1;
                    i = 0;
                    break;
                }  else if (!part_match) {
                    match_pos = left ? match_pos - 1 : match_pos + 1;
                }
            }
        } else if (pattern[match_bits - i] == 'C') {
            while((match_pos > 0 && left) || (match_pos < length && !left)) {
                if (((*(uint64_t *)(data + match_pos * unit_size) & pattern_mask) != 0) !=
                    ((*(uint64_t *)(data + (match_pos - 1) * unit_size) & pattern_mask) != 0)) {
                    part_match = true;
                    break;
                } else if ((match_pos == 1 && left) || (match_pos == length - 1 && !left)) {
                    unmatch = true;
                    part_match = false;
                    break;
                } else if (part_match) {
                    unmatch = true;
                    match_pos = left ? match_pos - 1 : match_pos + 1;
                    i = 0;
                    break;
                }  else if (!part_match) {
                    match_pos = left ? match_pos - 1 : match_pos + 1;
                }
            }
        }

        if (unmatch && !part_match)
            break;
        else if ((!unmatch && part_match) || !part_match)
            i++;
    }

    pos = match_pos;
    return !unmatch;
}

} // namespace dock
} // namespace pv
