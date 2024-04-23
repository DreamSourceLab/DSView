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

#include "keywordlineedit.h"
#include <QHBoxLayout>
#include <QTimer>
#include "../config/appconfig.h"
#include "../ui/langresource.h"
#include "../log.h"

namespace{
    QTimer *move_timer = NULL;
}

KeywordLineEdit::KeywordLineEdit(QWidget *parent, IKeywordActive *active)
:QLineEdit(parent)
{ 
    _bText = false;
    _active = active;
    this->ResetText();
}

void KeywordLineEdit::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton && _active != NULL){          
        _active->BeginEditKeyword();
    } 
    QLineEdit::mousePressEvent(e); 
}

void KeywordLineEdit::ResetText()
{ 
    if (_bText){
        return;         
    }

    this->setText(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_KEY_DECODER_SEARCH), "Decoder s_ann_search_editearch..."));
}

void KeywordLineEdit::SetInputText(QString text)
{
    _bText = true;
    this->setText(text);
}

//---------PopupLineEditInput
PopupLineEditInput::PopupLineEditInput(QWidget *parent)
    :QDialog(parent)
{  
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowSystemMenuHint);
    _line = NULL;

    QHBoxLayout *lay = new QHBoxLayout();
    lay->setContentsMargins(0,0,0,0);
    _textInput = new QLineEdit(this);
    lay->addWidget(_textInput);
    this->setLayout(lay);

    QFont font = this->font();
    font.setPointSizeF(AppConfig::Instance().appOptions.fontSize);
    _textInput->setFont(font);

    connect(_textInput, &QLineEdit::returnPressed, [=]() {
        InputRelease();        
    });
}

void PopupLineEditInput::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::ActivationChange){
        if (this->isActiveWindow() == false){
            InputRelease();
            return;
        }
    }

    QWidget::changeEvent(event);
}

void PopupLineEditInput::InputRelease()
{
    sig_inputEnd(_textInput->text());
    this->close();
    this->deleteLater();

    if (move_timer != NULL){
        move_timer->stop();
        delete move_timer;
        move_timer = NULL;
    }
}

void PopupLineEditInput::onCheckPostion()
{
    if (_line != NULL){
        QPoint p1 = _line->pos();
        QPoint p2 = _line->mapToGlobal(p1);
        int x = p2.x() - p1.x();
        int y = p2.y() - p1.y();

        QPoint p = this->pos();
        if (p.x() != x || p.y() != y){
            this->move(x, y);
        }       
    }
}

void PopupLineEditInput::Popup(QWidget *editline)
{
    assert(editline);
    _line = editline;

    _textInput->setFixedSize(editline->size());
    this->setFixedSize(editline->size());

    QPoint pt = mapToGlobal(editline->rect().bottomLeft());    

    QPoint p1 = editline->pos();
    QPoint p2 = editline->mapToGlobal(p1);
    int x = p2.x() - p1.x();
    int y = p2.y() - p1.y();
    this->move(x, y);

    _textInput->setFocus(); 
    _textInput->setCursorPosition(_textInput->text().length());

    if (move_timer != NULL){
        move_timer->stop();
        delete move_timer;
        move_timer = NULL;
    }
    move_timer = new QTimer(this);
    move_timer->setInterval(100);

    connect(move_timer, SIGNAL(timeout()), this, SLOT(onCheckPostion()));
    move_timer->start();

    this->show();
}

//---------PopupLineEdit
 PopupLineEdit::PopupLineEdit(QWidget *parent)
    :PopupLineEdit("", parent)
{
}

PopupLineEdit::PopupLineEdit(const QString &text, QWidget *parent)
    :QLineEdit(text, parent)
{
    _is_number_mode = false; 
}

void PopupLineEdit::mousePressEvent(QMouseEvent *event)
{
    showPupopInput();
    QLineEdit::mousePressEvent(event); 
}

void PopupLineEdit::showPupopInput()
{
#ifdef _WIN32
    PopupLineEditInput *input = new PopupLineEditInput(this);

    QString mask = this->inputMask();
    if (mask != ""){
        input->GetInput()->setInputMask(mask);
    }

    input->GetInput()->setMaxLength(this->maxLength());
    input->GetInput()->setText(this->text());
    input->setFont(this->font());

    if (_is_number_mode){
        QIntValidator *validator = new QIntValidator();
        input->GetInput()->setValidator(validator);
    }
    else{
        auto regular = this->validator();
        if (regular != NULL){
            input->GetInput()->setValidator(regular);
        }
    }

    _old_text = this->text();

    connect(input, SIGNAL(sig_inputEnd(QString)), this, SLOT(onPopupInputEditEnd(QString)));
    input->Popup(this);
#endif
}

void PopupLineEdit::onPopupInputEditEnd(QString text)
{
    this->setText(text);
    this->setFocus();
    this->setCursorPosition(this->text().length());

    if (text != _old_text){
        editingFinished();
    }    
}

bool PopupLineEdit::set_number_mode(bool isNumberMode)
{
    _is_number_mode = isNumberMode;

    if (_is_number_mode){
        QIntValidator *validator = new QIntValidator();
        setValidator(validator);
    }
    else{
        setValidator(NULL);
    }
}

int PopupLineEdit::value()
{
    assert(_is_number_mode);

    QString text = this->text();
    if (text != ""){
        return text.toInt();
    }
    return 0;
}

void PopupLineEdit::setValue(int value)
{
    this->setText(QString::number(value));    
}