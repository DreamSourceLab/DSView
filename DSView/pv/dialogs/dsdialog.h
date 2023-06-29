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


#ifndef DSVIEW_PV_DSDIALOG_H
#define DSVIEW_PV_DSDIALOG_H

#include <QDialog>
#include <QWidget>
#include <QVBoxLayout>
#include <QDialogButtonBox>

#include "../toolbars/titlebar.h"
#include "../interface/icallbacks.h"

class QDialogButtonBox;
 
 
namespace pv {
namespace dialogs {

    class Shadow;

//DSView any dialog base class
class DSDialog : public QDialog, IFontForm
{
	Q_OBJECT

public:
    DSDialog();
    DSDialog(QWidget *parent);
    DSDialog(QWidget *parent, bool hasClose);
    DSDialog(QWidget *parent, bool hasClose, bool bBaseButton);
    ~DSDialog();

    inline void SetCallback(IDlgCallback *callback){m_callback = callback;}
    inline QVBoxLayout *layout(){return _main_layout;}
    void setTitle(QString title);
    void reload(); 
    int exec();

    inline bool IsClickYes(){
        return _clickYes;
    }

    inline bool IsAccept(){
        return _clickYes;
    }

    void SetTitleSpace(int h);

    //IFontForm
    void update_font() override;

    void show();

protected:
    void accept();
    void reject();

private:
    void build_base(bool hasClose); 

private:
    QVBoxLayout         *_base_layout;
    QWidget             *_main_widget;
    QVBoxLayout         *_main_layout;
    toolbars::TitleBar  *_titlebar;
    Shadow              *_shadow;
    QDialogButtonBox    *_base_button; 
   
    QPoint              _startPos; 
    bool                 m_bBaseButton; 
    bool                _clickYes;
    QWidget             *_titleSpaceLine;

    IDlgCallback        *m_callback;
};

} // namespace dialogs
} // namespace pv

#endif // DSVIEW_PV_DSDIALOG_H
