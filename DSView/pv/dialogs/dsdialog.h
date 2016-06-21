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

#include "../toolbars/titlebar.h"

namespace pv {
namespace dialogs {

class DSDialog : public QDialog
{
	Q_OBJECT

public:
    DSDialog(QWidget *parent = 0, bool hasClose = false);
    QVBoxLayout *layout();
    QWidget *mainWidget();

    void setTitle(QString title);
    void reload(bool hasClose);

protected:
    void accept();
    void reject();
    //void mousePressEvent(QMouseEvent *event);
    //void mouseReleaseEvent(QMouseEvent *event);
    bool eventFilter(QObject *object, QEvent *event);
private:
    void build_main(bool hasClose);

private:
    QVBoxLayout *_layout;
    QVBoxLayout *_mlayout;
    QWidget *_main;
    toolbars::TitleBar *_titlebar;
    bool _moving;
    QPoint _startPos;
};

} // namespace dialogs
} // namespace pv

#endif // DSVIEW_PV_DSDIALOG_H
