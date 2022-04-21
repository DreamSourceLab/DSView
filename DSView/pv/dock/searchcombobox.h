#ifndef SEARCHCOMBOBOX_H
#define SEARCHCOMBOBOX_H

#include <QObject>
#include <QDialog>
#include <QWidget> 
#include <QMouseEvent>
#include <QPushButton>
#include <QString>
#include <vector>
#include <QEvent>
#include <QScrollArea>

class ISearchItemClick{
public:
    virtual void OnItemClick(void *sender, void *data_handle)=0;
};
 
//----------------------ComboButtonItem
class ComboButtonItem : public QPushButton
 {
    Q_OBJECT
public:
    ComboButtonItem(QWidget *parent, ISearchItemClick *click, void *data_handle);

protected:
    void mousePressEvent(QMouseEvent *e);

private:
    void *_data_handle;
    ISearchItemClick *_click;
 };

//----------------------SearchDataItem

class SearchDataItem{
public:
    QString     _id;
    QString     _name;
    void        *_data_handle;
    QWidget     *_control;
};

//----------------------SearchComboBox

class SearchComboBox : public QDialog, ISearchItemClick
{
    Q_OBJECT
public:
    explicit SearchComboBox(QWidget *parent = nullptr);

    ~SearchComboBox();

    void ShowDlg(QWidget *editline);

    void AddDataItem(QString id, QString name, void *data_handle);

    inline void SetItemClickHandle(ISearchItemClick *click){
        _item_click = click;        
    }

protected: 
    void changeEvent(QEvent *event);

private slots:
    void on_keyword_changed(const QString &value);
 
private:
    //ISearchItemClick
    void OnItemClick(void *sender, void *data_handle);

private: 
    bool        _bShow;
    std::vector<SearchDataItem*> _items;
    ISearchItemClick    *_item_click;
    QScrollArea     *_scroll;
};

#endif // SEARCHCOMBOBOX_H
