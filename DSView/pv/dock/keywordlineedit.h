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
#include <QMouseEvent>
#include <QWidget>
#include <QDialog>

class IKeywordActive{
public:
    virtual void BeginEditKeyword()=0;
};

class KeywordLineEdit : public QLineEdit
 {
    Q_OBJECT

public:
    KeywordLineEdit(QWidget *parent, IKeywordActive *active);

    void ResetText();

    void SetInputText(QString text);

protected:
    void mousePressEvent(QMouseEvent *e); 

private:
    IKeywordActive  *_active;
    bool            _bText;
};

//--------------SimpleKeywordLineEdit
class SimpleKeywordLineEdit : public QLineEdit
{
    Q_OBJECT

public:
    SimpleKeywordLineEdit(QWidget *parent);

    inline void EnableCatchKeyPress(bool enabled){
        _is_catch_keypress = enabled;
    }

signals:
    void sig_click();

protected:
    void mousePressEvent(QMouseEvent *e); 
    void keyPressEvent(QKeyEvent *e) override;

private:
    bool      _is_catch_keypress;
};

//--DecoderSearchInput
class DecoderSearchInput : public QDialog
{
 Q_OBJECT

public:
    explicit DecoderSearchInput(QWidget *parent = nullptr);

    QString GetText();
    void SetText(QString text);

    void Popup(QWidget *editline);

signals:
    void sig_inputEnd(QString text);

protected: 
    void changeEvent(QEvent *event) override;

    void InputRelease();

private:
    QLineEdit   *_textInput;};

#endif
