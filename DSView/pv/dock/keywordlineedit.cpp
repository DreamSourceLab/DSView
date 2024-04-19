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
#include "../config/appconfig.h"
#include "../ui/langresource.h"

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

//----------------SimpleKeywordLineEdit
 SimpleKeywordLineEdit::SimpleKeywordLineEdit(QWidget *parent)
    :QLineEdit(parent)
{
    _is_catch_keypress = false;
}

void SimpleKeywordLineEdit::mousePressEvent(QMouseEvent *e)
{
    sig_click();
    QLineEdit::mousePressEvent(e); 
}

void SimpleKeywordLineEdit::keyPressEvent(QKeyEvent *e)
{
    if (_is_catch_keypress){
        sig_click();
    }
    QLineEdit::keyPressEvent(e);
}

//----------------------DecoderSearchInput

DecoderSearchInput::DecoderSearchInput(QWidget *parent)
    :QDialog(parent)
{  
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowSystemMenuHint);

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

QString DecoderSearchInput::GetText()
{
    return _textInput->text();
}

void DecoderSearchInput::SetText(QString text)
{
    _textInput->setText(text);
}

void DecoderSearchInput::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::ActivationChange){
        if (this->isActiveWindow() == false){
            InputRelease();
            return;
        }
    }

    QWidget::changeEvent(event);
}

void DecoderSearchInput::InputRelease()
{
    sig_inputEnd(_textInput->text());
    this->close();
    this->deleteLater();
}

void DecoderSearchInput::Popup(QWidget *editline)
{
    assert(editline);

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
    this->show();
}
