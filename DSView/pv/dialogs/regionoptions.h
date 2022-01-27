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


#ifndef DSVIEW_PV_REGIONOPTIONS_H
#define DSVIEW_PV_REGIONOPTIONS_H

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
 
#include "../toolbars/titlebar.h"
#include "dsdialog.h"
#include "../ui/dscombobox.h"

namespace pv {

class SigSession;

namespace view {
class View;
}

namespace dialogs {

class RegionOptions : public DSDialog
{
    Q_OBJECT
private:
    static const QString RegionStart;
    static const QString RegionEnd;

public:
    RegionOptions(view::View *view, SigSession *session, QWidget *parent = 0);

private slots:
    void set_region();

private:
    SigSession *_session;
    view::View *_view;

    DsComboBox *_start_comboBox;
    DsComboBox *_end_comboBox;

    QDialogButtonBox _button_box;

};

} // namespace dialogs
} // namespace pv

#endif // DSVIEW_PV_REGIONOPTIONS_H
