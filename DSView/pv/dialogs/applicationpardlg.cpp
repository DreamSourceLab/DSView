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

#include "applicationpardlg.h"
#include "dsdialog.h"
#include <QFormLayout>
#include <QCheckBox>
#include <QString>
#include "../config/appconfig.h"

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
    dlg.setTitle("Application options");
    dlg.setMinimumSize(300, 200);
    QFormLayout &lay = *(new QFormLayout());
    lay.setContentsMargins(0,20,0,30);

    //show config
    AppConfig &app = AppConfig::Instance();

    QCheckBox *ck_quickScroll = new QCheckBox();
    ck_quickScroll->setChecked(app._appOptions.quickScroll);
    lay.addRow("Quick scroll", ck_quickScroll);
    dlg.layout()->addLayout(&lay);  
     
    dlg.exec();

    bool ret = dlg.IsClickYes();

    //save config
    if (ret){
        app._appOptions.quickScroll = ck_quickScroll->isChecked();
        app.SaveApp();
    }
   
   return ret;
}
 

} //
}//

