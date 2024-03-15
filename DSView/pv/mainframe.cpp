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

#include "mainframe.h"

#include "toolbars/titlebar.h"
#include "dialogs/dsmessagebox.h"
#include "dialogs/dsdialog.h"
#include "mainwindow.h"

#include <QVBoxLayout>
#include <QEvent>
#include <QMouseEvent>
#include <QHoverEvent>
#include <QPixmap>
#include <QPainter>
#include <QLabel>
#include <QDialogButtonBox>
#include <QBitmap>
#include <QResizeEvent>
#include <QDesktopServices>
#include <QPushButton>
#include <QMessageBox> 
#include <QScreen>
#include <QApplication>
#include <QFile> 
#include <QGuiApplication>
#include <QFont>
#include <algorithm>
#include <QWindow>

 #if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
 #include <QDesktopWidget>
 #endif

#include "dsvdef.h"
#include "config/appconfig.h"
#include "ui/msgbox.h"
#include "appcontrol.h"
#include "ui/langresource.h"
#include "log.h"
#include "dialogs/dsdialog.h"
#include "ui/popupdlglist.h"


#ifdef _WIN32
#include "winnativewidget.h"
#endif

namespace pv {

MainFrame::MainFrame()
{
    _layout = NULL;
    _bDraging = false;
    _hit_border = None;
    _freezing = false; 
    _titleBar = NULL;
    _mainWindow = NULL;
    _is_native_title = false;
    _is_resize_ready = false;   
    _parentNativeWidget = NULL;
    _mainWindow = NULL; 
    _move_start_screen = NULL; 

    _left   = NULL;
    _right  = NULL;
    _top    = NULL;
    _bottom = NULL;
    _top_left = NULL;
    _top_right = NULL;
    _bottom_left = NULL;
    _bottom_right = NULL;

    AppControl::Instance()->SetTopWindow(this);
  
   bool isWin32 = false;

#ifdef _WIN32
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowSystemMenuHint | Qt::WindowMinMaxButtonsHint);
    _is_native_title = true;
    _taskBtn = NULL;
    isWin32 = true;
#else
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowSystemMenuHint);
    setAttribute(Qt::WA_TranslucentBackground);
    _is_native_title = false;
#endif

#ifdef _WIN32
    if (!_is_native_title){
        setAttribute(Qt::WA_TranslucentBackground);
    }
#endif
  
   setMinimumWidth(MainWindow::Min_Width);
   setMinimumHeight(MainWindow::Min_Height);  
  
    // Set the window icon
    QIcon icon;
    icon.addFile(QString::fromUtf8(":/icons/logo.svg"), QSize(), QIcon::Normal, QIcon::Off);
    setWindowIcon(icon);
    
    _titleBar = new toolbars::TitleBar(true, this, this, false);
    _mainWindow = new MainWindow(_titleBar, this);
    _mainWindow->setWindowFlags(Qt::Widget);

    QVBoxLayout *vbox = new QVBoxLayout();
    vbox->setContentsMargins(0,0,0,0);
    vbox->setSpacing(0);
    vbox->addWidget(_titleBar);
    vbox->addWidget(_mainWindow);

    _layout = new QGridLayout(this);
    _layout->setSpacing(0);
    _layout->setContentsMargins(0,0,0,0);
 

    if (!isWin32 || !_is_native_title)
    {
        _top_left = new widgets::Border (TopLeft, this);
        _top_left->setFixedSize(Margin, Margin);
        _top_left->installEventFilter(this);
        _top = new widgets::Border (Top, this);
        _top->setFixedHeight(Margin);
        _top->installEventFilter(this);
        _top_right = new widgets::Border (TopRight, this);
        _top_right->setFixedSize(Margin, Margin);
        _top_right->installEventFilter(this);

        _left = new widgets::Border (Left, this);
        _left->setFixedWidth(Margin);
        _left->installEventFilter(this);
        _right = new widgets::Border (Right, this);
        _right->setFixedWidth(Margin);
        _right->installEventFilter(this);

        _bottom_left = new widgets::Border (BottomLeft, this);
        _bottom_left->setFixedSize(Margin, Margin);
        _bottom_left->installEventFilter(this);
        _bottom = new widgets::Border (Bottom, this);
        _bottom->setFixedHeight(Margin);
        _bottom->installEventFilter(this);
        _bottom_right = new widgets::Border (BottomRight, this);
        _bottom_right->setFixedSize(Margin, Margin);
        _bottom_right->installEventFilter(this);

        _layout->addWidget(_top_left, 0, 0);
        _layout->addWidget(_top, 0, 1);
        _layout->addWidget(_top_right, 0, 2);
        _layout->addWidget(_left, 1, 0);
        _layout->addLayout(vbox, 1, 1);
        _layout->addWidget(_right, 1, 2);
        _layout->addWidget(_bottom_left, 2, 0);
        _layout->addWidget(_bottom, 2, 1);
        _layout->addWidget(_bottom_right, 2, 2);
    }
    else{
        _layout->addLayout(vbox, 0, 0);
    }

#ifdef _WIN32
    _taskBtn = new QWinTaskbarButton(this);
	connect(_mainWindow, SIGNAL(prgRate(int)), this, SLOT(setTaskbarProgress(int)));
#endif

    connect(&_timer, SIGNAL(timeout()), this, SLOT(unfreezing()));

    QTimer::singleShot(2000, this, [this](){
                _is_resize_ready = true;
            });

    installEventFilter(this);

    connect(this, SIGNAL(sig_ParentNativeEvent(int)), this, SLOT(OnParentNaitveWindowEvent(int)));

  
}
  
void MainFrame::MoveWindow(int x, int y)
{
#ifdef _WIN32
    if (_parentNativeWidget != NULL){
        int k =  window()->devicePixelRatio(); 
        _parentNativeWidget->Move(x * k, y * k);
        return;
    }
#endif

    move(x, y);
}

QPoint MainFrame::GetParentPos()
{  
#ifdef _WIN32
    if (_parentNativeWidget != NULL){
        RECT rc;
        int k =  window()->devicePixelRatio(); 
        GetWindowRect(_parentNativeWidget->Handle(), &rc); 
        return QPoint(rc.left / k, rc.top / k);
    }
#endif
    
    return pos();
}

bool MainFrame::ParentIsMaxsized()
{
    return IsMaxsized();
}

void MainFrame::MoveBegin()
{
#ifdef _WIN32
     if (_parentNativeWidget != NULL){
        _parentNativeWidget->SetMovingFlag(true);
        _move_start_screen = _parentNativeWidget->GetPointScreen();
     }
#endif
}
 
void MainFrame::MoveEnd()
{
    QRect rc = GetFormRegion();

    if (rc.top() < 0){
        dsv_info("The top is out of range.");
        rc.setY(0);
        SetFormRegion(rc.x(), rc.y(), rc.width(), rc.height());
    }

    saveNormalRegion();

#ifdef _WIN32
    if (_parentNativeWidget != NULL){
        auto scr = _parentNativeWidget->GetPointScreen();
        if (scr != _move_start_screen){ 
            _parentNativeWidget->UpdateChildDpi();
            _parentNativeWidget->ResizeSelf();
        }

        _parentNativeWidget->ResizeChild();
        _parentNativeWidget->MoveShadow();
        _parentNativeWidget->SetMovingFlag(false);
    }    
#endif
}

void MainFrame::OnParentNativeEvent(ParentNativeEvent msg)
{
    sig_ParentNativeEvent((int)msg);
}

void MainFrame::OnParentNaitveWindowEvent(int msg)
{
#ifdef _WIN32
    if (_parentNativeWidget != NULL){
        if (msg == PARENT_EVENT_DISPLAY_CHANGED){
            QTimer::singleShot(100, this, [this](){
                PopupDlgList::TryCloseAllByScreenChanged();
                _parentNativeWidget->OnDisplayChanged();
            });
        }
    }
#endif
}

void MainFrame::OnMoveWinShadow()
{
#ifdef _WIN32
    if (_parentNativeWidget != NULL){
        _parentNativeWidget->MoveShadow();
    }
#endif
}
 
void MainFrame::resizeEvent(QResizeEvent *event)
{
    QFrame::resizeEvent(event);

    if (_layout == NULL){
        return;
    }

    if (IsMaxsized()) {
        hide_border();
    }
    else if(IsNormalsized()) {
        show_border();
    }

    _titleBar->setRestoreButton(IsMaxsized());
    _layout->update();
}

void MainFrame::closeEvent(QCloseEvent *event)
{ 
    writeSettings();

    if (_mainWindow->able_to_close()){
        
#ifdef _WIN32
        if (_parentNativeWidget != NULL){
            _parentNativeWidget->SetChildWidget(NULL);
            _parentNativeWidget->Show(false);
        }
#endif

        event->accept();
    }
    else{
        event->ignore();
    }
}

void MainFrame::unfreezing()
{
    _freezing = false;
}

void MainFrame::hide_border()
{  
    if (_top_left == NULL)
        return;

    _top_left->setVisible(false);
    _top_right->setVisible(false);
    _top->setVisible(false);
    _left->setVisible(false);
    _right->setVisible(false);
    _bottom_left->setVisible(false);
    _bottom->setVisible(false);
    _bottom_right->setVisible(false);
}

void MainFrame::show_border()
{  
    if (_top_left == NULL)
        return;

    _top_left->setVisible(true);
    _top_right->setVisible(true);
    _top->setVisible(true);
    _left->setVisible(true);
    _right->setVisible(true);
    _bottom_left->setVisible(true);
    _bottom->setVisible(true);
    _bottom_right->setVisible(true);
}

void MainFrame::showNormal()
{
    show_border(); 
    
#ifdef _WIN32
    if (_parentNativeWidget){
        _parentNativeWidget->ShowNormal();
        return;
    }
#endif

    QFrame::showNormal();
}

void MainFrame::showMaximized()
{ 
    hide_border();
 
#ifdef _WIN32
    if (_parentNativeWidget){
        _parentNativeWidget->ShowMax();
        return;
    }
#endif

    QFrame::showMaximized(); 
}

void MainFrame::showMinimized()
{  
#ifdef _WIN32
    if (_parentNativeWidget){
        _parentNativeWidget->ShowMin();
        return;
    }
#endif

    QFrame::showMinimized();
}

void MainFrame::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::WindowStateChange && _is_resize_ready) {     
        //dsv_info("Window state changed.");
        QWindowStateChangeEvent *stateChangeEvent = static_cast<QWindowStateChangeEvent*>(event);
        if (stateChangeEvent->oldState() & Qt::WindowMaximized 
                && !(windowState() & Qt::WindowMaximized)) {
            
        }       
    }
    QFrame::changeEvent(event);
}

bool MainFrame::eventFilter(QObject *object, QEvent *event)
{ 
    const QEvent::Type type = event->type();
    const QMouseEvent *const mouse_event = (QMouseEvent*)event;
    int newWidth = 0;
    int newHeight = 0;
    int newLeft = 0;
    int newTop = 0;

    if (type != QEvent::MouseMove 
        && type != QEvent::MouseButtonPress 
        && type != QEvent::MouseButtonRelease
        && type != QEvent::Leave){
        return QFrame::eventFilter(object, event);
    }

    //when window is maximized, or is moving, call return 
    if (IsMaxsized() || IsMoving()){
       return QFrame::eventFilter(object, event);
    }
 
    if (!_bDraging && type == QEvent::MouseMove && (!(mouse_event->buttons() | Qt::NoButton))){
           if (object == _top_left) {
                _hit_border = TopLeft;
                setCursor(Qt::SizeFDiagCursor);
            } else if (object == _bottom_right) {
                _hit_border = BottomRight;
                setCursor(Qt::SizeFDiagCursor);
            } else if (object == _top_right) {
                _hit_border = TopRight;
                setCursor(Qt::SizeBDiagCursor);
            } else if (object == _bottom_left) {
                _hit_border = BottomLeft;
                setCursor(Qt::SizeBDiagCursor);
            } else if (object == _left) {
                _hit_border = Left;
                setCursor(Qt::SizeHorCursor);
            } else if (object == _right) {
                _hit_border = Right;
                setCursor(Qt::SizeHorCursor);
            } else if (object == _bottom) {
                _hit_border = Bottom;
                setCursor(Qt::SizeVerCursor);
            } else if (object == _top) {
                _hit_border = Top;
                setCursor(Qt::SizeVerCursor);
            } else {
                _hit_border = None;
                setCursor(Qt::ArrowCursor);
            }

            return QFrame::eventFilter(object, event);
    }

  if (type == QEvent::MouseMove) {
 
        QPoint pt;
        int k = 1;

#ifdef _WIN32 
        POINT p;
        GetCursorPos(&p);
        pt.setX(p.x);
        pt.setY(p.y);

        if (_parentNativeWidget != NULL){
            auto scr = _parentNativeWidget->GetPointScreen();
            assert(scr);
            k = scr->devicePixelRatio();
        }
#else
        pt = mouse_event->globalPos(); 
#endif 
        int datX = pt.x() - _clickPos.x();
        int datY = pt.y() - _clickPos.y();
        datX /= k;
        datY /= k;
        
        int l = _dragStartRegion.left();
        int t = _dragStartRegion.top();
        int r = _dragStartRegion.right();
        int b = _dragStartRegion.bottom();

         if(mouse_event->buttons().testFlag(Qt::LeftButton)) {

            // Do nothing this time.
            if (_freezing){
                return QFrame::eventFilter(object, event);         
            }

            int minW = MainWindow::Min_Width;
            int minH = MainWindow::Min_Height;
          
            switch (_hit_border) {
                case TopLeft:
                    l += datX;
                    t += datY;
                    if (r - l < minW)
                        l = r - minW;
                    if (b - t < minH)
                        t = b - minH;
                   break;

                case BottomLeft:
                    l += datX;
                    b += datY;
                    if (r - l < minW)
                        l = r - minW;
                    if (b - t < minH)
                        b = t + minH;
                   break;

                case TopRight:
                    r += datX;
                    t += datY;
                    if (r - l < minW)
                        r = l + minW;
                    if (b - t < minH)
                        t = b - minH;
                   break;

                case BottomRight:
                    r += datX;
                    b += datY;
                    if (r - l < minW)
                        r = l + minW;
                    if (b - t < minH)
                        b = t + minH;
                   break;

                case Left:
                    l += datX;
                    if (r - l < minW)
                        l = r - minW;                    
                   break;

                case Right:
                    r += datX; 
                    if (r - l < minW)
                        r = l + minW;                     
                   break;

                case Top: 
                    t += datY; 
                    if (b - t < minH)
                        t = b - minH;                    
                   break;

                case Bottom: 
                    b += datY; 
                    if (b - t < minH)
                        b = t + minH;                    
                   break;

                default:
                    r = l;
                   break;
            }

            if (r != l){
                SetFormRegion(l, t, r-l, b-t);
                saveNormalRegion();
            }            
         
            return true;
        }
    }
    else if (type == QEvent::MouseButtonPress) {
        if (mouse_event->button() == Qt::LeftButton) 
        if (_hit_border != None)
            _bDraging = true;
        _timer.start(50); 

#ifdef _WIN32 
        POINT p;
        GetCursorPos(&p);
        _clickPos.setX(p.x);
        _clickPos.setY(p.y);
#else
        _clickPos = mouse_event->globalPos();
#endif 

        _dragStartRegion = GetFormRegion();
    } 
    else if (type == QEvent::MouseButtonRelease) {
        if (mouse_event->button() == Qt::LeftButton) {         
            _bDraging = false;
            _timer.stop(); 
        }
    }
    else if (!_bDraging && type == QEvent::Leave) {
        _hit_border = None;
        setCursor(Qt::ArrowCursor);
    } 
    
    return QFrame::eventFilter(object, event);
}

void MainFrame::saveNormalRegion()
{
    if (!_is_resize_ready){
        return;
    } 
    if (!IsNormalsized()){
        return;
    } 

    AppConfig &app = AppConfig::Instance();  

#ifdef _WIN32
    if (_parentNativeWidget != NULL){
        RECT rc; 
        auto scr = _parentNativeWidget->GetPointScreen();
        assert(scr);
        int k = scr->devicePixelRatio();
        
        GetWindowRect(_parentNativeWidget->Handle(), &rc);
        app.frameOptions.left = rc.left / k;
        app.frameOptions.top = rc.top / k;
        app.frameOptions.right = rc.right / k;
        app.frameOptions.bottom = rc.bottom / k;
        app.frameOptions.x = rc.left / k;
        app.frameOptions.y = rc.top / k; 
    }
#endif

    if (_parentNativeWidget == NULL){
        QRect rc = geometry();
        app.frameOptions.left = rc.left();
        app.frameOptions.top = rc.top();
        app.frameOptions.right = rc.right();
        app.frameOptions.bottom = rc.bottom();
        app.frameOptions.x = rc.left();
        app.frameOptions.y = rc.top(); 
    }
}

void MainFrame::writeSettings()
{  
    AppConfig &app = AppConfig::Instance();
    app.frameOptions.isMax = IsMaxsized();

    if (IsNormalsized() && isVisible()){
        saveNormalRegion();
    }
    app.SaveFrame();

    int x = app.frameOptions.x;
    int y = app.frameOptions.y;
    int w = app.frameOptions.right - app.frameOptions.left;
    int h = app.frameOptions.bottom - app.frameOptions.top;

    dsv_info("Save form, x:%d, y:%d, w:%d, h:%d", x, y, w, h);
}

void MainFrame::ShowFormInit()
{ 
    ReadSettings();

    if (_initWndInfo.r.y < 0){
        _initWndInfo.r.y = 0;
    }

    int x = _initWndInfo.r.x;
    int y = _initWndInfo.r.y;
    int w = _initWndInfo.r.w;
    int h = _initWndInfo.r.h;

    if (w % 2 == 0){
        w++;
    }
    if (h % 2 == 0){
        h++;
    }

    dsv_info("Init form, x:%d, y:%d, w:%d, h:%d, k:%d",
             x, y, w, h, _initWndInfo.k);

    bool isWin32 = false;

#ifdef _WIN32
    isWin32 = true;
#endif

    if (_initWndInfo.isMaxSize){
        move(x, y);
        showMaximized();
    }
    else{
        move(x, y);
        resize(w, h);
    }

    QFrame::show();
  
 #ifdef _WIN32
    if (_is_native_title){
        AttachNativeWindow();
    }
#endif 
}

void MainFrame::AttachNativeWindow()
{
#ifdef _WIN32 

    assert(_parentNativeWidget == NULL);

    int k = _initWndInfo.k;
   
    int x = _normalRegion.x;
    int y = _normalRegion.y;
    int w = _normalRegion.w;
    int h = _normalRegion.h;

    x *= k;
    y *= k;
    w *= k;
    h *= k;
 
    WinNativeWidget *nativeWindow = new WinNativeWidget(x, y, w, h);
    nativeWindow->setGeometry(x, y, w, h);

    if (nativeWindow->Handle() == NULL){
        dsv_info("ERROR: native window is invalid.");
        return;
    }
  
    //check the normal region
    QScreen *scr = nativeWindow->GetPointScreen();
    if (scr != NULL){
        QRect full_rc = scr->availableGeometry();

        if (full_rc.width() - _normalRegion.w < 100 && !_initWndInfo.isMaxSize)
        {  
            int w1 = full_rc.width() / 1.5;
            int h1 = full_rc.height() / 1.5;
            int x1 = full_rc.left() + (full_rc.width() - w1) / 2;
            int y1 = full_rc.top() + (full_rc.height() - h1) / 2; 
            nativeWindow->setGeometry(x1 * k, y1 * k, w1 * k, h1 *k);

            dsv_info("%s:Reset the normal region, x:%d, y:%d, w:%d, h:%d",
                        scr->name().toStdString().c_str(),
                        x1, y1, w1, h1);
        }
    }
    else{
        dsv_info("ERROR: get point screen error.");
    }

    setWindowFlags(Qt::FramelessWindowHint);
    setProperty("_q_embedded_native_parent_handle", (WId)nativeWindow->Handle());
    SetWindowLong((HWND)winId(), GWL_STYLE, WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
    SetParent((HWND)winId(), nativeWindow->Handle());

    QEvent e(QEvent::EmbeddingControl);
    QApplication::sendEvent(this, &e);

    if (_initWndInfo.isMaxSize){
        nativeWindow->ShowMax();
    }
    else{
        nativeWindow->Show(true);
    }
  
    nativeWindow->SetChildWidget(this);
    nativeWindow->SetNativeEventCallback(this);
    _parentNativeWidget = nativeWindow;

    auto shandow = _parentNativeWidget->Shadow();
    if (shandow){
        shandow->SetFrame(this);
        connect(shandow, SIGNAL(showSignal()), this, SLOT(OnMoveWinShadow()));
    }
 
    QTimer::singleShot(50, this, [this](){           
            move(0,0);
            setVisible(true);            
            _parentNativeWidget->UpdateChildDpi();
            _parentNativeWidget->ResizeChild(); 
            _parentNativeWidget->BeginShowShadow();        
        });
     
#endif
}

void MainFrame::SetFormRegion(int x, int y, int w, int h)
{
   #ifdef _WIN32

   if (_parentNativeWidget != NULL){
        auto scr = _parentNativeWidget->GetPointScreen();
        assert(scr); 
        int k = scr->devicePixelRatio(); 
        
        x *= k;
        y *= k;
        w *= k;
        h *= k;
 
        _parentNativeWidget->setGeometry(x, y, w, h);

        return;
   } 
   #endif
  
   setGeometry(x, y, w, h);
}

QRect MainFrame::GetFormRegion()
{
    QRect rc;

#ifdef _WIN32

    if (_parentNativeWidget != NULL){
        auto scr = _parentNativeWidget->GetPointScreen();
        assert(scr);
        int k = scr->devicePixelRatio();
        RECT r; 
        GetWindowRect(_parentNativeWidget->Handle(), &r); 
        int x = r.left;
        int y = r.top;
        int w = r.right-r.left;
        int h = r.bottom-r.top;
        rc = QRect(x/k, y/k, w/k, h/k);
    }
    else{
        rc = geometry();
    }
#else
    rc = geometry();  
#endif

    return rc;
}

bool MainFrame::IsMaxsized()
{
#ifdef _WIN32
    if (_parentNativeWidget != NULL){
        return _parentNativeWidget->IsMaxsized();
    }
#endif

    return QFrame::isMaximized();
}

bool MainFrame::IsNormalsized()
{
#ifdef _WIN32
    if (_parentNativeWidget != NULL){
        return _parentNativeWidget->IsNormalsized();
    }
#endif

    if (!QFrame::isMaximized() && !QFrame::isMinimized()){
        return true;
    }
    return false;
}

bool MainFrame::IsMoving()
{
    return _titleBar->IsMoving();
}

int MainFrame::GetDevicePixelRatio()
{ 
#ifdef _WIN32
    if (_parentNativeWidget != NULL){
        auto scr = _parentNativeWidget->GetPointScreen();
        assert(scr);
        return scr->devicePixelRatio();
    }
#endif

    return windowHandle()->devicePixelRatio();
}

void MainFrame::ReadSettings()
{
    AppConfig &app = AppConfig::Instance(); 
   
    if (app.frameOptions.language > 0){
        _mainWindow->switchLanguage(app.frameOptions.language);
    }

    int left = app.frameOptions.left;
    int top = app.frameOptions.top;
    int right = app.frameOptions.right;
    int bottom = app.frameOptions.bottom;
    int x = app.frameOptions.x;
    int y = app.frameOptions.y; 

    if (x == NO_POINT_VALUE){
        x = left;
        y = top;
    }

    dsv_info("Read region info, x:%d, y:%d, w:%d, h:%d, isMax:%d",
            x, y, right-left, bottom-top, app.frameOptions.isMax);

    bool bReset = false;
    int scrIndex = -1;

    QRect winRc = {x, y, right-left, bottom-top};

    for (int i=0; i<QGuiApplication::screens().size(); i++){
        QRect rc  = QGuiApplication::screens().at(i)->availableGeometry();
        QRect inrc = rc.intersected(winRc);
        QString name = QGuiApplication::screens().at(i)->name();

        dsv_info("Screen name:%s, region, left:%d, top:%d, right:%d, bottom:%d",
            name.toStdString().c_str(), rc.left(), rc.top(), rc.right(), rc.bottom() );
         
        if (scrIndex == -1 && (inrc.width() > 10 || inrc.height() > 10)){
            scrIndex = i;
        }
    }

    if (scrIndex == -1){
        bReset = true;
    }
    else{        
        QRect rc = QGuiApplication::screens().at(scrIndex)->availableGeometry();
        QRect inrc = rc.intersected(winRc);
        if (inrc.width() < 70 || inrc.height() < 70 || winRc.width() < 300){
            bReset = true;
        }
    }

    _initWndInfo.r.x = 0;
    _initWndInfo.r.y = 0;
    _initWndInfo.r.w = 0;
    _initWndInfo.r.h = 0;
    _initWndInfo.isMaxSize = false;
    int k = 1;

    if (app.frameOptions.isMax)
    {
        QRect rc;
        QString scrName;

        if (scrIndex == -1){
            rc = QGuiApplication::primaryScreen()->availableGeometry();
            scrName = QGuiApplication::primaryScreen()->name();
            k = QGuiApplication::primaryScreen()->devicePixelRatio();
        }
        else{
            rc = QGuiApplication::screens().at(scrIndex)->availableGeometry();
            scrName = QGuiApplication::screens().at(scrIndex)->name();
            k = QGuiApplication::screens().at(scrIndex)->devicePixelRatio();
        }

        _initWndInfo.r.x = rc.left();
        _initWndInfo.r.y = rc.top();
        _initWndInfo.r.w = rc.width();
        _initWndInfo.r.h = rc.height();
        _initWndInfo.isMaxSize = true;

        _normalRegion.x = x;
        _normalRegion.y = y;
        _normalRegion.w = right-left;
        _normalRegion.h = bottom-top;

        QRect inrc = rc.intersected(winRc);
        if (inrc.width() < 70 || inrc.height() < 70 || winRc.width() < 300){
            _normalRegion.x = rc.left() + (rc.width() - _normalRegion.w) / 2;
            _normalRegion.y = rc.top() + (rc.height() - _normalRegion.h) / 2;
        }

        dsv_info("Show as max, screen:%s, x:%d, y:%d, w:%d, h:%d", 
            scrName.toStdString().c_str(), rc.x(), rc.y(), rc.width(), rc.height());
    }
    else if (bReset)
    { 
        QRect rc;
        QString scrName;

        if (scrIndex == -1){
            rc = QGuiApplication::primaryScreen()->availableGeometry();
            scrName = QGuiApplication::primaryScreen()->name();
            k = QGuiApplication::primaryScreen()->devicePixelRatio();
        }
        else{
            rc = QGuiApplication::screens().at(scrIndex)->availableGeometry();
            scrName = QGuiApplication::screens().at(scrIndex)->name();
            k = QGuiApplication::screens().at(scrIndex)->devicePixelRatio();
        }

        int w = rc.width() / 1.5;
        int h = rc.height() / 1.5;
        const int x0 = rc.left() + (rc.width() - w) / 2;
        const int y0 = rc.top() + (rc.height() - h) / 2;

        _initWndInfo.r.x = x0;
        _initWndInfo.r.y = y0;
        _initWndInfo.r.w = w;
        _initWndInfo.r.h = h;
        _normalRegion = _initWndInfo.r;

        dsv_info("Reset, screen:%s, x:%d, y:%d, w:%d, h:%d", 
            scrName.toStdString().c_str(), rc.left(), rc.top(), rc.width(), rc.height());
    }
    else
    {
#ifdef _WIN32
        if (y != NO_POINT_VALUE){
            _initWndInfo.r.x = x;
            _initWndInfo.r.y = y;
        }
        else{
            _initWndInfo.r.x = left;
            _initWndInfo.r.y = top;
        }
#else
        _initWndInfo.r.x = left;
        _initWndInfo.r.y = top;
#endif 
        _initWndInfo.r.w = right - left;
        _initWndInfo.r.h = bottom - top;
        _normalRegion = _initWndInfo.r;
        QString scrName = QGuiApplication::screens().at(scrIndex)->name();
        QRect rc =  QGuiApplication::screens().at(scrIndex)->availableGeometry();
        k = QGuiApplication::screens().at(scrIndex)->devicePixelRatio();
 
        dsv_info("Restore, screen:%s, x:%d, y:%d, w:%d, h:%d", 
            scrName.toStdString().c_str() ,rc.left(), rc.top(), rc.width(), rc.height());
    }

    dsv_info("Normal region, x:%d, y:%d, w:%d, h:%d",  
       _normalRegion.x, _normalRegion.y, _normalRegion.w, _normalRegion.h);

    // restore dockwidgets
    _mainWindow->restore_dock();
    _titleBar->setRestoreButton(app.frameOptions.isMax);
    _initWndInfo.k = k;
}

#ifdef _WIN32
void MainFrame::showEvent(QShowEvent *event)
{
    // Taskbar Progress Effert for Win7 and Above
    if (_taskBtn && _taskBtn->window() == NULL) {
        _taskBtn->setWindow(windowHandle());
        _taskPrg = _taskBtn->progress();
    }
    event->accept();
}
#endif

void MainFrame::setTaskbarProgress(int progress)
{
#ifdef _WIN32
    if (progress > 0) {
        _taskPrg->setVisible(true);
        _taskPrg->setValue(progress);
    } else {
        _taskPrg->setVisible(false);
    }
#else
	(void)progress;
#endif
}

void MainFrame::ShowHelpDocAsync()
{
    QTimer::singleShot(300, this, [this](){
                show_doc();
            });
}

void MainFrame::show_doc()
{
     AppConfig &app = AppConfig::Instance(); 
     int lan = app.frameOptions.language;
      
    if (app.userHistory.showDocuments) {
        dialogs::DSDialog dlg(this, true);
        dlg.setTitle(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_DOCUMENT), "Document"));

        QString path = GetAppDataDir() + "/showDoc" + QString::number(lan)+ ".png";
        if (!QFile::exists(path)){
            path = ":/icons/showDoc"+QString::number(lan)+".png";
        }

        QLabel tipsLabel;
        tipsLabel.setPixmap(path);

        QMessageBox msg;
        msg.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowSystemMenuHint);
        msg.setContentsMargins(0, 0, 0, 0);
       
        QPushButton *noMoreButton = msg.addButton(L_S(STR_PAGE_MSG, S_ID(IDS_MSG_NOT_SHOW_AGAIN), "Not Show Again"), QMessageBox::ActionRole);
        msg.addButton(L_S(STR_PAGE_MSG, S_ID(IDS_MSG_IGNORE), "Ignore"), QMessageBox::ActionRole);
        QPushButton *openButton = msg.addButton(L_S(STR_PAGE_MSG, S_ID(IDS_MSG_OPEN), "Open"), QMessageBox::ActionRole);

        QVBoxLayout layout;
        layout.addWidget(&tipsLabel);
        layout.addWidget(&msg, 0, Qt::AlignRight);
        layout.setContentsMargins(0, 0, 0, 0);

        dlg.layout()->addLayout(&layout);
        connect(&msg, SIGNAL(buttonClicked(QAbstractButton*)), &dlg, SLOT(accept()));

        dlg.exec();

        if (msg.clickedButton() == openButton) {
            _mainWindow->openDoc();
        }
        if (msg.clickedButton() == noMoreButton){
            app.userHistory.showDocuments = false;
            app.SaveHistory();
        }   
    }
}

} // namespace pv
