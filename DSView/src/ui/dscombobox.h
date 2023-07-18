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

#ifndef DSCOMBOBOX_H
#define DSCOMBOBOX_H

#include <QComboBox>
#include <QKeyEvent>

class DsComboBox : public QComboBox
{
    Q_OBJECT

public:
    explicit DsComboBox(QWidget *parent = nullptr);

    void addItem(const QString &atext, const QVariant &userData = QVariant());

public:
    void showPopup() override;

    void hidePopup() override;

    inline bool  IsPopup(){
        return _bPopup;
    } 

private:
    int     _contentWidth;
    bool    _bPopup;
};



#endif // DSCOMBOBOX_H
