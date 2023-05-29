/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 * 
 * Copyright (C) 2021 DreamSourceLab <support@dreamsourcelab.com>
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

#include "applicationpardlg.h"
#include "dsdialog.h"
#include <QFormLayout>
#include <QCheckBox>
#include <QString>
#include "../config/appconfig.h"

#include "../ui/langresource.h"
#include "../appcontrol.h"
#include "../sigsession.h"

namespace pv
{
namespace dialogs
{

ApplicationParamDlg::ApplicationParamDlg()
{
   
}

ApplicationParamDlg::~ApplicationParamDlg()
{
}

bool ApplicationParamDlg::ShowDlg(QWidget *parent)
{
    DSDialog dlg(parent, true, true);
    dlg.setTitle(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_DISPLAY_OPTIONS), "Display options"));
    dlg.setMinimumSize(300, 200);
    QFormLayout &lay = *(new QFormLayout());
    lay.setContentsMargins(0,20,0,30);

    //show config
    AppConfig &app = AppConfig::Instance();

    int mode = AppControl::Instance()->GetSession()->get_device()->get_work_mode();

    QCheckBox *ck_quickScroll = new QCheckBox();
    ck_quickScroll->setChecked(app._appOptions.quickScroll);

    QCheckBox *ck_trigInMid = new QCheckBox();
    ck_trigInMid->setChecked(app._appOptions.trigPosDisplayInMid);

    if (mode == LOGIC){
        lay.addRow(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_QUICK_SCROLL), "Quick scroll"), ck_quickScroll);
    }
    else if (mode == DSO){
        lay.addRow(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_TRIG_DISPLAY_MIDDLE), "Tig pos in middle"), ck_trigInMid);
    }

    dlg.layout()->addLayout(&lay);  
     
    dlg.exec();

    bool ret = dlg.IsClickYes();

    //save config
    if (ret){
        app._appOptions.quickScroll = ck_quickScroll->isChecked();
        app._appOptions.trigPosDisplayInMid = ck_trigInMid->isChecked();

        app.SaveApp();
    }
   
   return ret;
}
 

} //
}//

