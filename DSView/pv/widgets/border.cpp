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

#include "border.h"
#include "../mainframe.h"
 
#include <QPainter>
#include <QLinearGradient>
#include <QRadialGradient>
#include "../config/appconfig.h"

namespace pv {
namespace widgets {

const QColor Border::dark_border0 = QColor(80, 80, 80, 255);
const QColor Border::dark_border1 = QColor(48, 47, 47, 200);
const QColor Border::dark_border2 = QColor(48, 47, 47, 150);
const QColor Border::dark_border3 = QColor(48, 47, 47, 100);
const QColor Border::dark_border4 = QColor(48, 47, 47, 10);

const QColor Border::light_border0 = QColor(100, 100, 100, 255);
const QColor Border::light_border1 = QColor(150, 150, 150, 150);
const QColor Border::light_border2 = QColor(150, 150, 150, 100);
const QColor Border::light_border3 = QColor(150, 150, 150, 50);
const QColor Border::light_border4 = QColor(150, 150, 150, 0);

Border::Border(int type, QWidget *parent) :
    QWidget(parent),
    _type(type)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);
}

void Border::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setPen(Qt::NoPen);
    painter.setRenderHint(QPainter::Antialiasing, true);
    QLinearGradient linearGrad(QPointF(width(), height()), QPointF(0, 0));
    AppConfig &app = AppConfig::Instance(); 
    QString style = app.frameOptions.style;

    if (style == THEME_STYLE_DARK) {
        linearGrad.setColorAt(0, dark_border0);
        linearGrad.setColorAt(0.25, dark_border1);
        linearGrad.setColorAt(0.5, dark_border2);
        linearGrad.setColorAt(0.75, dark_border3);
        linearGrad.setColorAt(1, dark_border4);
    } else {
        linearGrad.setColorAt(0, light_border0);
        linearGrad.setColorAt(0.25, light_border1);
        linearGrad.setColorAt(0.5, light_border2);
        linearGrad.setColorAt(0.75, light_border3);
        linearGrad.setColorAt(1, light_border4);
    }

    QRadialGradient radialGrad(QPointF(0, 0), width());
    if (style == THEME_STYLE_DARK) {
        radialGrad.setColorAt(0, dark_border0);
        radialGrad.setColorAt(0.25, dark_border1);
        radialGrad.setColorAt(0.5, dark_border2);
        radialGrad.setColorAt(0.75, dark_border3);
        radialGrad.setColorAt(1, dark_border4);
    } else {
        radialGrad.setColorAt(0, light_border0);
        radialGrad.setColorAt(0.25, light_border1);
        radialGrad.setColorAt(0.5, light_border2);
        radialGrad.setColorAt(0.75, light_border3);
        radialGrad.setColorAt(1, light_border4);
    }

    if (_type == pv::MainFrame::TopLeft) {
        QRectF rectangle(0, 0, width()*2, height()*2);
        radialGrad.setCenter(QPointF(width(), height()));
        radialGrad.setFocalPoint(QPointF(width(), height()));
        painter.setBrush(QBrush(radialGrad));
        painter.drawPie(rectangle, 90 * 16, 180 * 16);
    } else if (_type == pv::MainFrame::TopRight) {
        QRectF rectangle(-width(), 0, width()*2, height()*2);
        radialGrad.setCenter(QPointF(0, height()));
        radialGrad.setFocalPoint(QPointF(0, height()));
        painter.setBrush(QBrush(radialGrad));
        painter.drawPie(rectangle, 0 * 16, 90 * 16);
    } else if (_type == pv::MainFrame::BottomLeft) {
        QRectF rectangle(0, -height(), width()*2, height()*2);
        radialGrad.setCenter(QPointF(width(), 0));
        radialGrad.setFocalPoint(QPointF(width(), 0));
        painter.setBrush(QBrush(radialGrad));
        painter.drawPie(rectangle, 180 * 16, 270 * 16);
    } else if (_type == pv::MainFrame::BottomRight) {
        QRectF rectangle(-width(), -height(), width()*2, height()*2);
        radialGrad.setCenter(QPointF(0, 0));
        radialGrad.setFocalPoint(QPointF(0, 0));
        painter.setBrush(QBrush(radialGrad));
        painter.drawPie(rectangle, 270 * 16, 360 * 16);
    } else if (_type == pv::MainFrame::Top) {
        linearGrad.setStart(QPointF(0, height()));
        linearGrad.setFinalStop(QPointF(0, 0));
        painter.setBrush(QBrush(linearGrad));
        painter.drawRect(rect());
    } else if (_type == pv::MainFrame::Bottom) {
        linearGrad.setStart(QPointF(0, 0));
        linearGrad.setFinalStop(QPointF(0, height()));
        painter.setBrush(QBrush(linearGrad));
        painter.drawRect(rect());
    } else if (_type == pv::MainFrame::Left) {
        linearGrad.setStart(QPointF(width(), 0));
        linearGrad.setFinalStop(QPointF(0, 0));
        painter.setBrush(QBrush(linearGrad));
        painter.drawRect(rect());
    } else if (_type == pv::MainFrame::Right) {
        linearGrad.setStart(QPointF(0, 0));
        linearGrad.setFinalStop(QPointF(width(), 0));
        painter.setBrush(QBrush(linearGrad));
        painter.drawRect(rect());
    }
}

void Border::leaveEvent(QEvent*)
{
    //setCursor(Qt::ArrowCursor);
}

} // namespace widgets
} // namespace pv
