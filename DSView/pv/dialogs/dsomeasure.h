/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
 * Copyright (C) 2015 DreamSourceLab <support@dreamsourcelab.com>
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


#ifndef DSVIEW_PV_DSOMEASURE_H
#define DSVIEW_PV_DSOMEASURE_H

#include <QVBoxLayout>
#include <QToolButton>
#include <QDialogButtonBox>
#include <QTabWidget>
 

#include "../view/dsosignal.h"
#include "../toolbars/titlebar.h"
#include "dsdialog.h"

namespace pv {

class SigSession;

namespace view {
class View;
}

namespace dialogs {

class DsoMeasure : public DSDialog
{
	Q_OBJECT

public:
    DsoMeasure(SigSession *session, view::View &parent, unsigned int position, int last_sig_index);

    ~DsoMeasure();

    static QString get_ms_icon(int ms_type);
    static QString get_ms_text(int ms_type);

private:
    void add_measure(QWidget *widget, const view::DsoSignal *dsoSig);

private slots:
    void set_measure(bool en);
    void reset();

protected:
	void accept();
    void reject();

private:
    SigSession *_session;
    view::View &_view;
    unsigned int _position;
 
    QDialogButtonBox _button_box;
    QTabWidget *_measure_tab;
    QVBoxLayout _layout; 
};

} // namespace dialogs
} // namespace pv

#endif // DSVIEW_PV_DSOMEASURE_H
