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

#ifndef DSVIEW_PV_VIEW_VIEWSTATUS_H
#define DSVIEW_PV_VIEW_VIEWSTATUS_H

#include <QWidget>
#include <QLabel>
#include <QDateTime>
#include <QPushButton>
#include <QToolButton> 
#include <libsigrok.h> 

namespace pv {

class SigSession;

namespace view {
class View;
class DsoSignal;

//created by View
class ViewStatus : public QWidget
{
    Q_OBJECT

public:
   ViewStatus(SigSession *session, View &parent);

public: 

    void paintEvent(QPaintEvent *);
    void mousePressEvent(QMouseEvent *);

    void set_measure(unsigned int index, bool canceled,
                     int sig_index, enum DSO_MEASURE_TYPE ms_type);

    QJsonArray get_session();
    void load_session(QJsonArray meausre_array, int version);
    void set_capture_status(bool triggered, int progess);

public slots:
    void clear();
    void reload();
    void repeat_unshow();
    void set_trig_time(QDateTime time);
    void set_rle_depth(uint64_t depth);    

private:
    SigSession *_session;
    View &_view;
    int _hit_rect;

    QString _trig_time;
    QString _rle_depth;
    QString _capture_status;

    int _last_sig_index;
    std::vector<std::tuple<QRect, int, enum DSO_MEASURE_TYPE>> _mrects;
};

} // namespace view
} // namespace pv

#endif // DSVIEW_PV_VIEW_VIEWSTATUS_H
