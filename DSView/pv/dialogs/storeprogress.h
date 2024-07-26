/*
 * This file is part of the PulseView project.
 * DSView is based on PulseView.
 *
 * Copyright (C) 2014 Joel Holdsworth <joel@airwebreathe.org.uk>
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

#ifndef DSVIEW_PV_DIALOGS_SAVEPROGRESS_H
#define DSVIEW_PV_DIALOGS_SAVEPROGRESS_H
 
#include <QTimer>
#include "../storesession.h"
#include "../dialogs/dsdialog.h" 
#include "../interface/icallbacks.h"

class QTextEdit;
class QRadioButton;
class QGridLayout;
class QPushButton;
class QWidget;
class QComboBox;
class QProgressBar;

namespace pv {
    namespace view {
        class View;
    }
}

namespace pv {

class SigSession;

namespace dialogs {

class StoreProgress : public DSDialog
{
	Q_OBJECT

public:
    StoreProgress(SigSession *session,
        QWidget *parent = 0);

	virtual ~StoreProgress();

    inline void SetView(view::View *view){
        _view = view;
    }

    void save_run(ISessionDataGetter *getter);
    void export_run();
 
private:
    void reject();
    void accept();
	void show_error();
    void closeEvent(QCloseEvent* event) override;
    void keyPressEvent(QKeyEvent *event) override;
    
private slots:
	void on_progress_updated();
    void on_timeout();
    void on_change_file();
    void on_ck_origin(bool ck);
    void on_ck_compress(bool ck);

private:
    pv::StoreSession    *_store_session;
    QProgressBar        *_progress;
    bool                _isExport;
    QTextEdit           *_fileLab;
    QRadioButton        *_ckOrigin;
    QRadioButton        *_ckCompress;
    QPushButton         *_openButton;
    QGridLayout         *_grid;
    QWidget             *_space;
    QComboBox           *_start_cursor;
    QComboBox           *_end_cursor;
    view::View          *_view; 
    bool                _is_done;
    QTimer              m_timer;
    QString             _file_path;
};

} // dialogs
} // pv

#endif // PULSEVIEW_PV_DIALOGS_SAVEPROGRESS_H
