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
#include "mainwindow.h"

#include <QVBoxLayout>
#include <QEvent>
#include <QMouseEvent>
#include <QHoverEvent>
#include <QPixmap>
#include <QPainter>
#include <QBitmap>
#include <QResizeEvent>
#include <QDesktopWidget>
#include <QApplication>

#include <algorithm>

namespace pv {

MainFrame::MainFrame(DeviceManager &device_manager,
    const char *open_file_name)
{
    setAttribute(Qt::WA_TranslucentBackground);
    // Make this a borderless window which can't
    // be resized or moved via the window system
    setWindowFlags(Qt::FramelessWindowHint);
    setMinimumHeight(680);
    setMinimumWidth(800);
    //resize(1024, 768);

    // Set the window icon
    QIcon icon;
    icon.addFile(QString::fromUtf8(":/icons/logo.png"),
        QSize(), QIcon::Normal, QIcon::Off);
    setWindowIcon(icon);

    _moving = false;
    _startPos = None;
    _freezing = false;
    _minimized = false;

    // MainWindow
    _mainWindow = new MainWindow(device_manager, open_file_name, this);
    _mainWindow->setWindowFlags(Qt::Widget);

    // Title
    _titleBar = new toolbars::TitleBar(true, this);
    _titleBar->installEventFilter(this);
    _titleBar->setTitle(_mainWindow->windowTitle());

    QVBoxLayout *vbox = new QVBoxLayout();
    vbox->setMargin(0);
    vbox->setSpacing(0);
    vbox->addWidget(_titleBar);
    vbox->addWidget(_mainWindow);

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

    _layout = new QGridLayout(this);
    _layout->setMargin(0);
    _layout->setSpacing(0);
    _layout->addWidget(_top_left, 0, 0);
    _layout->addWidget(_top, 0, 1);
    _layout->addWidget(_top_right, 0, 2);
    _layout->addWidget(_left, 1, 0);
    _layout->addLayout(vbox, 1, 1);
    _layout->addWidget(_right, 1, 2);
    _layout->addWidget(_bottom_left, 2, 0);
    _layout->addWidget(_bottom, 2, 1);
    _layout->addWidget(_bottom_right, 2, 2);

    connect(&_timer, SIGNAL(timeout()), this, SLOT(unfreezing()));
    readSettings();
}

void MainFrame::changeEvent(QEvent* event)
{
    QFrame::changeEvent(event);
     QWindowStateChangeEvent* win_event = static_cast< QWindowStateChangeEvent* >(event);
     if(win_event->type() == QEvent::WindowStateChange) {
        if (win_event->oldState() & Qt::WindowMinimized) {
             if (_minimized) {
                 readSettings();
                 _minimized = false;
             }
         }
     }
}


void MainFrame::resizeEvent(QResizeEvent *event)
{
    QFrame::resizeEvent(event);
    if (isMaximized()) {
        hide_border();
    } else {
        show_border();
    }
    _titleBar->setRestoreButton(isMaximized());
    _layout->update();
}

void MainFrame::closeEvent(QCloseEvent *event)
{
    _mainWindow->session_save();
    writeSettings();
    event->accept();
}

void MainFrame::unfreezing()
{
    _freezing = false;
}

void MainFrame::hide_border()
{
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
    QFrame::showNormal();
}

void MainFrame::showMaximized()
{
    hide_border();
    QFrame::showMaximized();
}

void MainFrame::showMinimized()
{
    _minimized = true;
    writeSettings();
    QFrame::showMinimized();
}

bool MainFrame::eventFilter(QObject *object, QEvent *event)
{
    const QEvent::Type type = event->type();
    const QMouseEvent *const mouse_event = (QMouseEvent*)event;
    int newWidth;
    int newHeight;
    int newLeft;
    int newTop;

    if (type == QEvent::MouseMove && !isMaximized()) {
        if (!(mouse_event->buttons() || Qt::NoButton)) {
            if (object == _top_left) {
                _startPos = TopLeft;
                setCursor(Qt::SizeFDiagCursor);
            } else if (object == _bottom_right) {
                _startPos = BottomRight;
                setCursor(Qt::SizeFDiagCursor);
            } else if (object == _top_right) {
                _startPos = TopRight;
                setCursor(Qt::SizeBDiagCursor);
            } else if (object == _bottom_left) {
                _startPos = BottomLeft;
                setCursor(Qt::SizeBDiagCursor);
            } else if (object == _left) {
                _startPos = Left;
                setCursor(Qt::SizeHorCursor);
            } else if (object == _right) {
                _startPos = Right;
                setCursor(Qt::SizeHorCursor);
            } else if (object == _bottom) {
                _startPos = Bottom;
                setCursor(Qt::SizeVerCursor);
            } else if (object == _top) {
                _startPos = Top;
                setCursor(Qt::SizeVerCursor);
            } else {
                _startPos = None;
                setCursor(Qt::ArrowCursor);
            }
        } else if(mouse_event->buttons().testFlag(Qt::LeftButton)) {
            if (_moving) {
                this->move(mouse_event->globalPos() - _lastMousePosition);
            } else if (!_freezing) {
                switch (_startPos) {
                case TopLeft:
                    newWidth = std::max(_dragStartGeometry.right() - mouse_event->globalX(), minimumWidth());
                    newHeight = std::max(_dragStartGeometry.bottom() - mouse_event->globalY(), minimumHeight());
                    newLeft = geometry().left();
                    newTop = geometry().top();
                    if (newWidth > minimumWidth())
                        newLeft = mouse_event->globalX();
                    if (newHeight > minimumHeight())
                        newTop = mouse_event->globalY();
                    setGeometry(newLeft, newTop,
                                newWidth, newHeight);
                   break;
                case BottomLeft:
                    newWidth = std::max(_dragStartGeometry.right() - mouse_event->globalX(), minimumWidth());
                    newHeight = std::max(mouse_event->globalY() - _dragStartGeometry.top(), minimumHeight());
                    newLeft = geometry().left();
                    if (newWidth > minimumWidth())
                        newLeft = mouse_event->globalX();
                    setGeometry(newLeft, _dragStartGeometry.top(),
                                newWidth, newHeight);
                   break;
                case TopRight:
                    newWidth = std::max(mouse_event->globalX() - _dragStartGeometry.left(), minimumWidth());
                    newHeight = std::max(_dragStartGeometry.bottom() - mouse_event->globalY(), minimumHeight());
                    newTop = geometry().top();
                    if (newHeight > minimumHeight())
                        newTop = mouse_event->globalY();
                    setGeometry(_dragStartGeometry.left(), newTop,
                                newWidth, newHeight);
                   break;
                case BottomRight:
                    newWidth = std::max(mouse_event->globalX() - _dragStartGeometry.left(), minimumWidth());
                    newHeight = std::max(mouse_event->globalY() - _dragStartGeometry.top(), minimumHeight());
                    setGeometry(_dragStartGeometry.left(), _dragStartGeometry.top(),
                                newWidth, newHeight);
                   break;
                case Left:
                    newWidth = _dragStartGeometry.right() - mouse_event->globalX();
                    if (newWidth > minimumWidth())
                        setGeometry(mouse_event->globalX(), _dragStartGeometry.top(),
                                    newWidth, height());
                   break;
                case Right:
                    newWidth = mouse_event->globalX() - _dragStartGeometry.left();
                    if (newWidth > minimumWidth())
                        setGeometry(_dragStartGeometry.left(), _dragStartGeometry.top(),
                                    newWidth, height());
                   break;
                case Top:
                    newHeight = _dragStartGeometry.bottom() - mouse_event->globalY();
                    if (newHeight > minimumHeight())
                        setGeometry(_dragStartGeometry.left(), mouse_event->globalY(),
                                    width(), newHeight);
                   break;
                case Bottom:
                    newHeight = mouse_event->globalY() - _dragStartGeometry.top();
                    if (newHeight > minimumHeight())
                        setGeometry(_dragStartGeometry.left(), _dragStartGeometry.top(),
                                    width(), newHeight);
                   break;
                default:
                   break;
                }
                _freezing = true;
            } 
            return true;
        }
    } else if (type == QEvent::MouseButtonPress) {
        if (mouse_event->button() == Qt::LeftButton)
            if (_titleBar->rect().contains(mouse_event->pos()) &&
                _startPos == None) {
            _moving = true;
            _lastMousePosition = mouse_event->pos() +
                                 //QPoint(Margin, Margin) +
                                 QPoint(geometry().left() - frameGeometry().left(), frameGeometry().right() - geometry().right());
        }
        if (_startPos != None)
            _draging = true;
        _timer.start(50);
        _dragStartGeometry = geometry();
    } else if (type == QEvent::MouseButtonRelease) {
        if (mouse_event->button() == Qt::LeftButton) {
            _moving = false;
            _draging = false;
            _timer.stop();
        }
    } else if (!_draging && type == QEvent::Leave) {
        _startPos = None;
        setCursor(Qt::ArrowCursor);
    }

    return QObject::eventFilter(object, event);
}

void MainFrame::writeSettings()
{
    QSettings settings;

    settings.beginGroup("MainFrame");
    settings.setValue("size", size());
    settings.setValue("pos", pos() +
                      QPoint(geometry().left() - frameGeometry().left(), frameGeometry().right() - geometry().right()));
    settings.endGroup();
}

void MainFrame::readSettings()
{
    QSettings settings;
    QDesktopWidget* desktopWidget = QApplication::desktop();
    QRect deskRect = desktopWidget->availableGeometry();

    settings.beginGroup("MainFrame");
    QSize size = settings.value("size", QSize(minWidth, minHeight)).toSize();
    QPoint pos = settings.value("pos", QPoint((deskRect.width() - minWidth)/2, (deskRect.height() - minHeight)/2)).toPoint();
    settings.endGroup();

    if (size == deskRect.size()) {
        _titleBar->showMaxRestore();
    } else {
        resize(size);
        move(pos);
    }
}

} // namespace pv
