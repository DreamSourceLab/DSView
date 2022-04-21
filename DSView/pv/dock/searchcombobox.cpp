#include "searchcombobox.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPoint>
#include <QLineEdit>
#include <QScrollBar>
 

//----------------------ComboButtonItem

ComboButtonItem::ComboButtonItem(QWidget *parent, ISearchItemClick *click, void *data_handle)
:QPushButton(parent)
{
    _click = click;
    _data_handle = data_handle;
}

void ComboButtonItem::mousePressEvent(QMouseEvent *e)
{
    if (_click != NULL){
        _click->OnItemClick(this, _data_handle);
    }
}

//----------------------SearchComboBox

SearchComboBox::SearchComboBox(QWidget *parent)
    : QDialog(parent)
{ 
    _bShow = false;
    _item_click = NULL;

    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowSystemMenuHint);
    // setAttribute(Qt::WA_TranslucentBackground); 
}

SearchComboBox::~SearchComboBox(){
    //release resource
    for (auto o : _items){
        delete o;
    }
    _items.clear();  
}

void SearchComboBox::ShowDlg(QWidget *editline)
{
    if (_bShow)
        return;   

    _bShow = true;
    
    int w = 350; 
    int h = 450;

    if (editline != NULL){
       w = editline->width() + 7;
    } 

    this->setMinimumSize(w, h);
    this->setMaximumSize(w, h); 

    QVBoxLayout *grid = new QVBoxLayout(this);
    this->setLayout(grid);
    grid->setContentsMargins(0,0,0,0);
    grid->setAlignment(Qt::AlignTop);
    grid->setSpacing(2);

    QLineEdit *edit = new QLineEdit(this);
    edit->setMaximumWidth(this->width()); 
    grid->addWidget(edit);

    QWidget *panel = new QWidget(this);
    grid->addWidget(panel);

    panel->setContentsMargins(0,0,0,0);
    panel->setMinimumSize(w, h - edit->height() - 5);
    panel->setMaximumSize(w, h - edit->height() - 5); 

    QWidget *listPanel =  new QWidget(panel);
    listPanel->setMinimumSize(w-2, panel->height()); 
    listPanel->setMaximumWidth(w-2);

    QVBoxLayout *listLay = new QVBoxLayout(listPanel);
    listLay->setContentsMargins(2, 2, 20, 2);
    listLay->setSpacing(0);
    listLay->setAlignment(Qt::AlignTop);

    for(auto o : _items)
    { 
        ComboButtonItem *bt = new ComboButtonItem(panel, this, o);
        bt->setText(o->_name);
        bt->setObjectName("flat");
        bt->setMaximumWidth(w - 20);
        bt->setMinimumWidth(w - 20);
        o->_control = bt;
        listLay->addWidget(bt);
    } 
 
    _scroll = new QScrollArea(panel);
    _scroll->setWidget(listPanel);
   // pScrollArea->setStyleSheet("QScrollArea{border:none; background:red;}");
    _scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    if (editline != NULL)
    {
        QPoint p1 = editline->pos();
        QPoint p2 = editline->mapToGlobal(p1);
        int x = p2.x() - p1.x();
        int y = p2.y() - p1.y();
        this->move(x, y);       
    } 

    edit->setFocus();
    this->show();


    connect(edit, SIGNAL(textEdited(const QString &)), 
                    this, SLOT(on_keyword_changed(const QString &)));
}

void SearchComboBox::AddDataItem(QString id, QString name, void *data_handle)
{
    SearchDataItem *item = new SearchDataItem();
    item->_id = id;
    item->_name = name;
    item->_data_handle = data_handle;
    this->_items.push_back(item);
}

 void SearchComboBox::changeEvent(QEvent *event)
 {
    if (event->type() == QEvent::ActivationChange){
        if (this->isActiveWindow() == false){
            this->close();
            this->deleteLater();
            return;
        }
    }
    
    QWidget::changeEvent(event);
 }

 void SearchComboBox::OnItemClick(void *sender, void *data_handle)
 {
     if (data_handle != NULL && _item_click){
         SearchDataItem *item = (SearchDataItem*)data_handle;        
          this->close();
          ISearchItemClick *click = _item_click;
          this->deleteLater();
         click->OnItemClick(this, item->_data_handle);
     }
 }

 void SearchComboBox::on_keyword_changed(const QString &value)
 {
     for(auto o : _items)
     {
         if (value == "" 
            || o->_name.indexOf(value, 0, Qt::CaseInsensitive) >= 0
            || o->_id.indexOf(value, 0, Qt::CaseInsensitive) >= 0){
                if (o->_control->isHidden()){
                    o->_control->show();
                }
         }
         else if (o->_control->isHidden() == false){
             o->_control->hide();
         }
     }

    _scroll->verticalScrollBar()->setValue(0);
 }
