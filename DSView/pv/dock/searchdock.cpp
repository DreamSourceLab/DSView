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
#include "../dialogs/dsmessagebox.h"

#include <QObject>
#include <QPainter> 
#include <QRect>
#include <QMouseEvent>
#include <QFuture>
#include <QProgressDialog>
#include <QtConcurrent/QtConcurrent>
#include <stdint.h> 
#include "../config/appconfig.h"
#include "../ui/langresource.h"
#include "../ui/msgbox.h"
#include "../appcontrol.h"
#include "../ui/fn.h"

namespace pv {
namespace dock {

using namespace pv::view;
using namespace pv::widgets;

SearchDock::SearchDock(QWidget *parent, View &view, SigSession *session) :
    QWidget(parent),
    _session(session),
    _view(view)
{ 
    _search_button = new QPushButton(this);
    _search_button->setFixedWidth(_search_button->height());
    _search_button->setDisabled(true);

    QLineEdit *_search_parent = new QLineEdit(this);
    _search_parent->setVisible(false);
    _search_value = new FakeLineEdit(_search_parent);

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

    retranslateUi();

    update_font();

    connect(&_pre_button, SIGNAL(clicked()), this, SLOT(on_previous()));
    connect(&_nxt_button, SIGNAL(clicked()),this, SLOT(on_next()));
}

SearchDock::~SearchDock()
{
}

void SearchDock::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    else if (event->type() == QEvent::StyleChange)
        reStyle();
    QWidget::changeEvent(event);
}

void SearchDock::retranslateUi()
{
    _search_value->setPlaceholderText(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_SEARCH), "search"));
}

void SearchDock::reStyle()
{
    QString iconPath = GetIconPath();

    _pre_button.setIcon(QIcon(iconPath+"/pre.svg"));
    _nxt_button.setIcon(QIcon(iconPath+"/next.svg"));
    _search_button->setIcon(QIcon(iconPath+"/search.svg"));
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
    const auto snapshot = _session->get_snapshot(SR_CHANNEL_LOGIC);
    assert(snapshot);
    const auto logic_snapshot = dynamic_cast<data::LogicSnapshot*>(snapshot);

    if (logic_snapshot == NULL || logic_snapshot->empty()) {
        QString strMsg(L_S(STR_PAGE_MSG, S_ID(IDS_MSG_NO_SAMPLE_DATA), "No Sample data!"));
        MsgBox::Show(strMsg);        
        return;
    }

    const int64_t end = logic_snapshot->get_sample_count() - 1;
    last_pos = _view.get_search_pos();
    last_hit = _view.get_search_hit();
    if (last_pos == 0) {
        QString strMsg(L_S(STR_PAGE_MSG, S_ID(IDS_MSG_SEARCH_AT_START), "Search cursor at the start position!"));
        MsgBox::Show(strMsg);
        return;
    }
    else {
        QFuture<void> future;
        future = QtConcurrent::run([&]{
            last_pos -= last_hit;
            ret = logic_snapshot->pattern_search(0, end, last_pos, _pattern, false);
        });
        Qt::WindowFlags flags = Qt::CustomizeWindowHint;
        QProgressDialog dlg(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_SEARCH_PREVIOUS), "Search Previous..."),
                            L_S(STR_PAGE_DLG, S_ID(IDS_DLG_CANCEL), "Cancel"),0,0,this,flags);
        dlg.setWindowModality(Qt::WindowModal);
        dlg.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowSystemMenuHint |
                           Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint);
        dlg.setCancelButton(NULL);

        QFutureWatcher<void> watcher;
        connect(&watcher,SIGNAL(finished()),&dlg,SLOT(cancel()));
        watcher.setFuture(future);
        dlg.exec();

        if (!ret) {
            QString strMsg(L_S(STR_PAGE_MSG, S_ID(IDS_MSG_PATTERN_NOT_FOUND), "Pattern not found!"));
            MsgBox::Show(strMsg);
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
    const auto snapshot = _session->get_snapshot(SR_CHANNEL_LOGIC);
    assert(snapshot);
    const auto logic_snapshot = dynamic_cast<data::LogicSnapshot*>(snapshot);

    if (logic_snapshot == NULL || logic_snapshot->empty()) {
        QString strMsg(L_S(STR_PAGE_MSG, S_ID(IDS_MSG_NO_SAMPLE_DATA), "No Sample data!"));
        MsgBox::Show(strMsg);
        return;
    }

    const int64_t end = logic_snapshot->get_sample_count() - 1;
    last_pos = _view.get_search_pos() + _view.get_search_hit();
    if (last_pos >= end) {
        QString strMsg(L_S(STR_PAGE_MSG, S_ID(IDS_MSG_SEARCH_AT_END), "Search cursor at the end position!"));
        MsgBox::Show(strMsg);
        return;
    } else {
        QFuture<void> future;
        future = QtConcurrent::run([&]{
            ret = logic_snapshot->pattern_search(0, end, last_pos, _pattern, true);
        });
        Qt::WindowFlags flags = Qt::CustomizeWindowHint;
        QProgressDialog dlg(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_SEARCH_NEXT), "Search Next..."),
                            L_S(STR_PAGE_DLG, S_ID(IDS_DLG_CANCEL), "Cancel"),0,0,this,flags);
        dlg.setWindowModality(Qt::WindowModal);
        dlg.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowSystemMenuHint |
                           Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint);
        dlg.setCancelButton(NULL);

        QFutureWatcher<void> watcher;
        connect(&watcher,SIGNAL(finished()),&dlg,SLOT(cancel()));
        watcher.setFuture(future);
        dlg.exec();

        if (!ret) {
            QString strMsg(L_S(STR_PAGE_MSG, S_ID(IDS_MSG_PATTERN_NOT_FOUND), "Pattern not found!"));
            MsgBox::Show(strMsg);
            return;
        } else {
            _view.set_search_pos(last_pos, true);
        }
    }
}

void SearchDock::on_set()
{
    dialogs::Search dlg(this, _session, _pattern);
    connect(_session->device_event_object(), SIGNAL(device_updated()), &dlg, SLOT(reject()));

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
        //fm.width(search_label)
        int tw = fm.boundingRect(search_label).width();
        _search_value->setFixedWidth(tw + _search_button->width()+20);

        if (new_pattern != _pattern) {
            _view.set_search_pos(_view.get_search_pos(), false);
            _pattern = new_pattern;
        }
    }
}

void SearchDock::update_font()
{
    QFont font = this->font();
    font.setPointSizeF(AppConfig::Instance().appOptions.fontSize);
    _search_value->setFont(font);
}

} // namespace dock
} // namespace pv
