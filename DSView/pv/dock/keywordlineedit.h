/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
 * Copyright (C) 2022 DreamSourceLab <support@dreamsourcelab.com>
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

#ifndef KEY_WORD_LINE_EDIT_H
#define KEY_WORD_LINE_EDIT_H

#include <QObject>
#include <QLineEdit>
#include <QComboBox>
#include <QFocusEvent>

#define PROTOCOL_FIND_TITLE  "Protocol search..."

class KeywordLineEdit : public QLineEdit
 {
    Q_OBJECT

public:
    KeywordLineEdit(QComboBox *comboBox);

protected:
    void focusInEvent(QFocusEvent *e) override;
    void focusOutEvent(QFocusEvent *e) override;

private:
    QComboBox   *_comboBox;  
};


#endif
