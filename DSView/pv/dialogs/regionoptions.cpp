/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
 * Copyright (C) 2016 DreamSourceLab <support@dreamsourcelab.com>
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

#include "regionoptions.h"
 
#include "../sigsession.h"
#include "../view/cursor.h"
#include "../view/view.h"

#include "../ui/langresource.h"

using namespace boost;
using namespace std;

namespace pv {
namespace dialogs {

const QString RegionOptions::RegionStart = QT_TR_NOOP("Start");
const QString RegionOptions::RegionEnd = QT_TR_NOOP("End");

RegionOptions::RegionOptions(view::View *view, SigSession *session, QWidget *parent) :
    DSDialog(parent),
    _session(session),
    _view(view),
    _button_box(QDialogButtonBox::Ok,
        Qt::Horizontal, this)
{
    QHBoxLayout *hlayout = new QHBoxLayout();
    hlayout->setContentsMargins(0,0,0,0);
    hlayout->setSpacing(0);
    _start_comboBox = new DsComboBox(this);
    _end_comboBox = new DsComboBox(this);
    _start_comboBox->addItem(RegionStart);
    _end_comboBox->addItem(RegionEnd);

    if (_view) {
        int index = 1;
        auto &cursor_list = _view->get_cursorList();

        for(auto i = cursor_list.begin(); i != cursor_list.end(); i++) {
            QString curCursor = L_S(STR_PAGE_DLG, S_ID(IDS_DLG_CURSOR), "Cursor")+QString::number(index);
            _start_comboBox->addItem(curCursor);
            _end_comboBox->addItem(curCursor);
            index++;
        }
    }

    hlayout->addWidget(new QLabel("Start: ", this));
    hlayout->addWidget(_start_comboBox);
    hlayout->addWidget(new QLabel("    ", this));
    hlayout->addWidget(new QLabel("End: ", this));
    hlayout->addWidget(_end_comboBox);

    QVBoxLayout *vlayout = new QVBoxLayout();
    vlayout->addLayout(hlayout);
    vlayout->addWidget(&_button_box);

    layout()->addLayout(vlayout);
    setTitle(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_REGION), "Region"));

    connect(&_button_box, SIGNAL(accepted()), this, SLOT(set_region()));
    connect(_session->device_event_object(), SIGNAL(device_updated()), this, SLOT(reject()));

}

void RegionOptions::set_region()
{
    const uint64_t last_samples = _session->cur_samplelimits() - 1;
    const int index1 = _start_comboBox->currentIndex();
    const int index2 = _end_comboBox->currentIndex();
    uint64_t start, end;

    _session->set_save_start(0);
    _session->set_save_end(last_samples);

    if (index1 == 0) {
        start = 0;
    } else {
        start = _view->get_cursor_samples(index1-1);
    }
    if (index2 == 0) {
        end = last_samples;
    } else {
        end = _view->get_cursor_samples(index2-1);
    }

    if (start > last_samples)
        start = 0;
    if (end > last_samples)
        end = last_samples;

    _session->set_save_start(min(start, end));
    _session->set_save_end(max(start, end));

    QDialog::accept();
}

} // namespace dialogs
} // namespace pv
