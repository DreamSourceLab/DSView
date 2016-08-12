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

#include "dsapplication.h"

#include <QMessageBox>

DSApplication::DSApplication(int &argc, char **argv):
    QApplication(argc, argv)
{
}

bool DSApplication::notify(QObject *receiver_, QEvent *event_)
{
    try {
        return QApplication::notify(receiver_, event_);
    } catch ( std::exception& e ) {
        QMessageBox msg(NULL);
        msg.setText("Application Error");
        msg.setInformativeText(e.what());
        msg.setStandardButtons(QMessageBox::Ok);
        msg.setIcon(QMessageBox::Warning);
        msg.exec();
    } catch (...) {
        QMessageBox msg(NULL);
        msg.setText("Application Error");
        msg.setInformativeText("An unexpected error occurred");
        msg.setStandardButtons(QMessageBox::Ok);
        msg.setIcon(QMessageBox::Warning);
        msg.exec();
    }
    return false;
}
