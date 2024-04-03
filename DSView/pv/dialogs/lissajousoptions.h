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


#ifndef DSVIEW_PV_LISSAJOUSOPTIONS_H
#define DSVIEW_PV_LISSAJOUSOPTIONS_H

#include <QGridLayout> 
#include <QDialogButtonBox>
#include <QTabWidget>
#include <QGroupBox>
#include <QCheckBox>
#include <QRadioButton>
#include <QSlider>
#include "../view/dsosignal.h"
#include "../toolbars/titlebar.h"
#include "dsdialog.h"
#include "../ui/uimanager.h"


namespace pv {

class SigSession;

namespace view {
class View;
}

namespace dialogs {

class LissajousOptions : public DSDialog, public IUiWindow
{
	Q_OBJECT

private:
    static const int WellLen = SR_Kn(16);

public:
    LissajousOptions(SigSession *session, QWidget *parent);

    ~LissajousOptions();

private:    
    void retranslateUi();

    //IUiWindow
    void UpdateLanguage() override;
    void UpdateTheme() override;
    void UpdateFont() override;

protected:
	void accept();
    void reject();

private:
    SigSession *_session;

    QCheckBox *_enable;
    QGroupBox *_x_group;
    QGroupBox *_y_group;
    QSlider *_percent;
    QGridLayout *_layout;

    QVector<QRadioButton *> _x_radio;
    QVector<QRadioButton *> _y_radio;
    QDialogButtonBox _button_box;  
};

} // namespace dialogs
} // namespace pv

#endif // DSVIEW_PV_LISSAJOUSOPTIONS_H
