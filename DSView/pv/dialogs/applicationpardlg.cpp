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
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QString>
#include <QFontDatabase>
#include <QGroupBox>
#include <QLabel>
#include <vector>

#include "../config/appconfig.h"
#include "../ui/langresource.h"
#include "../appcontrol.h"
#include "../sigsession.h"
#include "../ui/dscombobox.h"
#include "../log.h"

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

void ApplicationParamDlg::bind_font_name_list(QComboBox *box, QString v)
{   
    int selDex = -1;

    QString defName(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_DEFAULT_FONT), "Default"));
    box->addItem(defName);

    QFontDatabase fDataBase;
    for (QString family : fDataBase.families()) {
        if (family.indexOf("[") == -1)
        {
            box->addItem(family);

            if (selDex == -1 && family == v){
                selDex = box->count() - 1;
            }
        }
    }

    if (selDex == -1)
        selDex = 0;

    box->setCurrentIndex(selDex);
}

void ApplicationParamDlg::bind_font_size_list(QComboBox *box, float size)
{   
    int selDex = -1;

    for(int i=9; i<=15; i++)
    {
        box->addItem(QString::number(i));
        if (i == size){
            selDex = box->count() - 1;
        }
    }
    if (selDex == -1)
        selDex = 0;
    box->setCurrentIndex(selDex);
}

bool ApplicationParamDlg::ShowDlg(QWidget *parent)
{
    DSDialog dlg(parent, true, true);
    dlg.setTitle(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_DISPLAY_OPTIONS), "Display options"));
    dlg.setMinimumSize(300, 230);
    QFormLayout &lay = *(new QFormLayout());
    lay.setContentsMargins(0,20,0,30);

    //show config
    AppConfig &app = AppConfig::Instance();

    int mode = AppControl::Instance()->GetSession()->get_device()->get_work_mode();

    QCheckBox *ck_quickScroll = new QCheckBox();
    ck_quickScroll->setChecked(app.appOptions.quickScroll);

    QCheckBox *ck_trigInMid = new QCheckBox();
    ck_trigInMid->setChecked(app.appOptions.trigPosDisplayInMid);

    QCheckBox *ck_profileBar = new QCheckBox();
    ck_profileBar->setChecked(app.appOptions.displayProfileInBar);

    QCheckBox *ck_abortData = new QCheckBox();
    ck_abortData->setChecked(app.appOptions.swapBackBufferAlways);

    lay.setHorizontalSpacing(8);

    if (mode == LOGIC){
        lay.addRow(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_QUICK_SCROLL), "Quick scroll"), ck_quickScroll);
        lay.addRow(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_USE_ABORT_DATA_REPEAT), "Used abort data"), ck_abortData);
    }
    else if (mode == DSO){
        lay.addRow(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_TRIG_DISPLAY_MIDDLE), "Tig pos in middle"), ck_trigInMid);
    }

    lay.addRow(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_DISPLAY_PROFILE_IN_BAR), "Profile in bar"), ck_profileBar);

    //----------------Font setting.
    std::vector<FontBindInfo> font_bind_list;
    FontBindInfo ft1 = {NULL, NULL, S_ID(IDS_DLG_TOOLBAR_FONT), "Toolbar", 
                            &app.fontOptions.toolbar.name, &app.fontOptions.toolbar.size};
    font_bind_list.push_back(ft1);

    FontBindInfo ft2 = {NULL, NULL, S_ID(IDS_DLG_CHANNEL_NAME_FONT), "Channel Name", 
                            &app.fontOptions.channelLabel.name, &app.fontOptions.channelLabel.size};
    font_bind_list.push_back(ft2);

    FontBindInfo ft3 = {NULL, NULL, S_ID(IDS_DLG_CHANNEL_BODY_FONT), "Channel Body", 
                            &app.fontOptions.channelBody.name, &app.fontOptions.channelBody.size};
    font_bind_list.push_back(ft3);

    FontBindInfo ft4 = {NULL, NULL, S_ID(IDS_DLG_RULER_FONT), "Ruler", 
                            &app.fontOptions.ruler.name, &app.fontOptions.ruler.size};
    font_bind_list.push_back(ft4);

    FontBindInfo ft5 = {NULL, NULL, S_ID(IDS_DLG_TITLE_FONT), "Titel", 
                            &app.fontOptions.title.name, &app.fontOptions.title.size};
    font_bind_list.push_back(ft5);

    FontBindInfo ft6 = {NULL, NULL, S_ID(IDS_DLG_OTHER_FONT), "Other", 
                            &app.fontOptions.other.name, &app.fontOptions.other.size};
    font_bind_list.push_back(ft6);

    QWidget *lineSpace = new QWidget();
    lineSpace->setFixedHeight(8);
    lay.addRow(lineSpace);

    QGroupBox *ftGroup = new QGroupBox();
    ftGroup->setTitle(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_FONT_GROUP), "Font"));
    QVBoxLayout *ftPannelLay = new QVBoxLayout();
    ftGroup->setLayout(ftPannelLay);
    ftPannelLay->setContentsMargins(5,15,5,10);

    for (FontBindInfo &inf : font_bind_list)
    {
        QWidget *ftWid = new QWidget();
        QHBoxLayout *ftLay = new QHBoxLayout();
        ftLay->setContentsMargins(0,0,0,0);
        ftWid->setLayout(ftLay);
        QString ftLb = LangResource::Instance()->get_lang_text(STR_PAGE_DLG, 
                            inf.lang_id.toUtf8().data(), inf.lang_def);
        ftLay->addWidget(new QLabel(ftLb));
        QComboBox *ftCbName = new DsComboBox();
        bind_font_name_list(ftCbName, *inf.ptr_name);
        ftLay->addWidget(ftCbName);
        QComboBox *ftCbSize = new DsComboBox();
        ftCbSize->setFixedWidth(50);
        bind_font_size_list(ftCbSize, *inf.ptr_size);
        ftLay->addWidget(ftCbSize);
        ftPannelLay->addWidget(ftWid);

        inf.name_box = ftCbName;
        inf.size_box = ftCbSize;
    } 

    lay.addRow(ftGroup);

    dlg.layout()->addLayout(&lay);  
     
    dlg.exec();

    bool ret = dlg.IsClickYes();

    //save config
    if (ret){

        bool bAppChanged = false;
        bool bFontChanged = false;

        if (app.appOptions.quickScroll != ck_quickScroll->isChecked()){
            app.appOptions.quickScroll = ck_quickScroll->isChecked();
            bAppChanged = true;
        }       
        if (app.appOptions.trigPosDisplayInMid != ck_trigInMid->isChecked()){
            app.appOptions.trigPosDisplayInMid = ck_trigInMid->isChecked();
            bAppChanged = true;
        }
        if (app.appOptions.displayProfileInBar != ck_profileBar->isChecked()){
            app.appOptions.displayProfileInBar = ck_profileBar->isChecked();
            bAppChanged = true;
        }
        if (app.appOptions.swapBackBufferAlways != ck_abortData->isChecked()){
            app.appOptions.swapBackBufferAlways = ck_abortData->isChecked();
            bAppChanged = true;
        }

        for (FontBindInfo &inf : font_bind_list)
        {
            QString oldName = *inf.ptr_name;
            float oldSize = *inf.ptr_size;

            if (inf.name_box->currentIndex() > 0)
                *inf.ptr_name = inf.name_box->currentText();
            else
                *inf.ptr_name = "";
            
            if (inf.size_box->currentIndex() >= 0)
                *inf.ptr_size = inf.size_box->currentText().toFloat();
            else
                *inf.ptr_size = 9;

            if (oldName != *inf.ptr_name || oldSize != *inf.ptr_size){
                bFontChanged = true;
            }
        }

        if (bFontChanged){
            app.SaveFont();
            AppControl::Instance()->GetSession()->broadcast_msg(DSV_MSG_FONT_OPTIONS_CHANGED);
        }

        if (bAppChanged){
            app.SaveApp();
            AppControl::Instance()->GetSession()->broadcast_msg(DSV_MSG_APP_OPTIONS_CHANGED);
        }       
    }
   
   return ret;
}
 

} //
}//

