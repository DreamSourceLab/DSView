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
#include <QWheelEvent>

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

//---------KeyLineEdit
class KeyLineEdit : public QLineEdit
{
    Q_OBJECT

public:
    explicit KeyLineEdit(QWidget *parent = nullptr);
    explicit KeyLineEdit(const QString &text, QWidget *parent = nullptr);
 
    inline bool is_number_mode(){
        return _is_number_mode;
    }

    virtual void setRange(int min, int max);

    void set_number_mode(bool isNumberMode);

    inline void enable_spin_mode(bool enabled){
        _is_spin_mode = enabled;
    }

    int value();

signals:
    void valueChanged(int v);

public slots:
    void setValue(int v);

private:
    void keyPressEvent(QKeyEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

protected:
    int         _min;
    int         _max;
    bool        _is_number_mode;
    bool        _is_spin_mode;
};

//---------PopupLineEditInput
class PopupLineEditInput : public QDialog
{
 Q_OBJECT

public:
    explicit PopupLineEditInput(QWidget *parent = nullptr);

    void Popup(QWidget *editline);

    inline KeyLineEdit* GetInput(){
        return _textInput;
    }

    inline void input_close(){
        InputRelease();
    }

signals:
    void sig_inputEnd(QString text);

private slots:
    void onCheckPositionTimeout(); 

protected: 
    void changeEvent(QEvent *event) override;
    void InputRelease();

private:
    KeyLineEdit     *_textInput;
    QWidget         *_line;
};

//---------PopupLineEdit
class PopupLineEdit : public KeyLineEdit
{
    Q_OBJECT

public:
    explicit PopupLineEdit(QWidget *parent = nullptr);
    explicit PopupLineEdit(const QString &text, QWidget *parent = nullptr);

    inline void set_instant_mode(bool isInstant){
        _is_instant = isInstant;
    }

    void show();
    void hide();

    void setRange(int min, int max) override;

private slots:
    void onPopupInputEditEnd(QString text);
    void onPopupInputValueChanged(int v);

protected:
    void mousePressEvent(QMouseEvent *event) override; 

private:
    void showPupopInput();    

private:
    QString     _old_text;
    bool        _is_instant;
    PopupLineEditInput  *_popup_input;
};

#endif
