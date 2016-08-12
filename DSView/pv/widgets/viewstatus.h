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

#ifndef DSVIEW_PV_WIDGETS_VIEWSTATUS_H
#define DSVIEW_PV_WIDGETS_VIEWSTATUS_H

#include <QWidget>
#include <QLabel>
#include "QDateTime"

namespace pv {

class SigSession;

namespace widgets {

class ViewStatus : public QWidget
{
    Q_OBJECT
public:
    explicit ViewStatus(QWidget *parent = 0);

    void paintEvent(QPaintEvent *);

signals:

public slots:
    void clear();
    void set_trig_time(QDateTime time);
    void set_rle_depth(uint64_t depth);

private:
    QString _trig_time;
    QString _rle_depth;
};

} // namespace widgets
} // namespace pv

#endif // DSVIEW_PV_WIDGETS_VIEWSTATUS_H
