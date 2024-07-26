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
#include <QSpinBox>
#include "../config/appconfig.h"
#include "../ui/langresource.h"
#include "../log.h"
#include "../dsvdef.h"

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

//---------KeyLineEdit
KeyLineEdit::KeyLineEdit(QWidget *parent)
    :KeyLineEdit("", parent)
{   
}

KeyLineEdit::KeyLineEdit(const QString &text, QWidget *parent)
    :QLineEdit(text, parent)
{
    _min = 0;
    _max = 0;
    _is_number_mode = false;
    _is_spin_mode = false;
}

void KeyLineEdit::keyPressEvent(QKeyEvent *event)
{  
    int key = event->key();
    QString old_text = this->text();

    QLineEdit::keyPressEvent(event);

    if (_is_number_mode && key >= '0' && key <= '9')
    { 
        QString new_text = text();

        if (new_text != ""){
            int v = new_text.toInt();

            if (_min != 0 || _max != 0){
                if (v < _min ){
                    v = _min;
                }
                else if (v > _max){
                    v = _max;
                }
            }

            new_text = QString::number(v);
            setText(new_text); //Maby need to restore the old value.

            if (new_text != old_text){               
                valueChanged(v);
            }
        }        
    }
}

void KeyLineEdit::wheelEvent(QWheelEvent *event)
{  
    if (_is_number_mode && _is_spin_mode)
    {
        QString new_text = text();

        if (new_text != "")
        {
            int v = new_text.toInt();
            int old_v = v;

            int delta = 0;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            int anglex = event->angleDelta().x();
            int angley = event->angleDelta().y();

            if (anglex == 0 || ABS_VAL(angley) >= ABS_VAL(anglex)){
                delta = angley;
            }
#else
            delta = event->delta();
#endif

            if (delta > 0){
                v++;
            }
            else{
                v--;
            }

            if (_min != 0 || _max != 0)
            {
                if (v < _min ){
                    v = _min;
                }
                else if (v > _max){
                    v = _max;
                }
            }
            
            if (v != old_v){
                setText(QString::number(v));
                valueChanged(v);
            }
        } 

        event->accept();
        return;
    } 

    QLineEdit::wheelEvent(event);
}

void KeyLineEdit::setValue(int v)
{   
    _is_number_mode = true;
    this->setText(QString::number(v));    
}

int KeyLineEdit::value()
{
    assert(_is_number_mode);

    QString text = this->text();
    if (text != ""){
        return text.toInt();
    }
    return 0;
}

void KeyLineEdit::setRange(int min, int max)
{
    _max = max;
    _min = min; 
    _is_spin_mode = true;
    set_number_mode(true);
}

void KeyLineEdit::set_number_mode(bool isNumberMode)
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

//---------PopupLineEditInput
PopupLineEditInput::PopupLineEditInput(QWidget *parent)
    :QDialog(parent)
{  
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowSystemMenuHint);
    _line = NULL;
 
    QHBoxLayout *lay = new QHBoxLayout();
    lay->setContentsMargins(0,0,0,0);
    _textInput = new KeyLineEdit(this);
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

void PopupLineEditInput::onCheckPositionTimeout()
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

    connect(move_timer, SIGNAL(timeout()), this, SLOT(onCheckPositionTimeout()));
    move_timer->start();

    this->show();
}

//---------PopupLineEdit
 PopupLineEdit::PopupLineEdit(QWidget *parent)
    :PopupLineEdit("", parent)
{
}

PopupLineEdit::PopupLineEdit(const QString &text, QWidget *parent)
    :KeyLineEdit(text, parent)
{
    _is_number_mode = false;
    _is_instant = false;
    _popup_input = NULL;
    _min = 0;
    _max = 0;
}

void PopupLineEdit::mousePressEvent(QMouseEvent *event)
{
    showPupopInput();
    KeyLineEdit::mousePressEvent(event); 
}

void PopupLineEdit::showPupopInput()
{
#ifdef _WIN32
    PopupLineEditInput *input = new PopupLineEditInput(this);
    auto line = input->GetInput();
 
    QString mask = this->inputMask();
    if (mask != ""){
        line->setInputMask(mask);
    }

    line->setMaxLength(this->maxLength());
    line->setText(this->text());
    line->setFont(this->font());
    line->set_number_mode(_is_number_mode);
   
    if (_is_number_mode){
        if (_max != 0 || _min != 0){
            line->setRange(_min, _max); 
        }        
    }
    else{
        auto regular = this->validator();
        if (regular != NULL){
            line->setValidator(regular);
        }
    }

    _old_text = this->text();
    _popup_input = input;

    connect(input, SIGNAL(sig_inputEnd(QString)), this, SLOT(onPopupInputEditEnd(QString)));

    if (_is_number_mode){
        connect(line, SIGNAL(valueChanged(int)), this, SLOT(onPopupInputValueChanged(int)));
    }

    input->Popup(this);
#endif
}

void PopupLineEdit::onPopupInputEditEnd(QString text)
{
    _popup_input = NULL;

    this->setText(text);
    this->setFocus();
    this->setCursorPosition(this->text().length());

    if (text != _old_text){
        setModified(true);
        editingFinished();

        if (_is_number_mode){
            valueChanged(value());
        }
    }    
}

void PopupLineEdit::onPopupInputValueChanged(int v)
{
    setValue(v);
    valueChanged(v);
}

void PopupLineEdit::show()
{
#ifdef _WIN32
    if (_is_instant){
        showPupopInput();
        return;
    }
#endif

    QLineEdit::show();
}

void PopupLineEdit::hide()
{
    if (_popup_input != NULL){
        _popup_input->input_close();
        _popup_input = NULL;
    }

    QLineEdit::hide();
}

void PopupLineEdit::setRange(int min, int max)
{
    KeyLineEdit::setRange(min, max);

    if (_popup_input != NULL){
        _popup_input->GetInput()->setRange(min, max);
    }
}
