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


#ifndef DSVIEW_PV_DSMESSAGEBOX_H
#define DSVIEW_PV_DSMESSAGEBOX_H

#include <QDialog>
#include <QWidget>
#include <QMessageBox>
#include <QVBoxLayout>

#include "../toolbars/titlebar.h"

namespace pv {
namespace dialogs {

     class Shadow;

class DSMessageBox : public QDialog
{
	Q_OBJECT

public:
    DSMessageBox(QWidget *parent, const QString title=0);

    ~DSMessageBox();

    QMessageBox *mBox(); 

    int exec();
    
    inline int IsYes(){return _bClickYes;}

protected:
    void accept();
    void reject();  

private slots:
    void on_button(QAbstractButton* btn);

private:
    QVBoxLayout         *_layout;
    QVBoxLayout         *_main_layout;
    QWidget             *_main_widget;
    QMessageBox         *_msg;
    toolbars::TitleBar  *_titlebar;
    Shadow              *_shadow;
 
    QPoint              _startPos;
    bool                _bClickYes;
};

} // namespace dialogs
} // namespace pv

#endif // DSVIEW_PV_DSMESSAGEBOX_H
