/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
 * Copyright (C) 2013 DreamSourceLab <support@dreamsourcelab.com>
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


#ifndef DSVIEW_PV_SEARCH_H
#define DSVIEW_PV_SEARCH_H

#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QDialogButtonBox>
#include "../sigsession.h"
#include <libsigrok4DSL/libsigrok.h>
#include "../toolbars/titlebar.h"
#include "dsdialog.h"
#include "../device/devinst.h"

#include <boost/shared_ptr.hpp>

namespace pv {
namespace dialogs {

class Search : public DSDialog
{
    Q_OBJECT

public:

    Search(QWidget *parent = 0, boost::shared_ptr<pv::device::DevInst> dev_inst = 0, QString pattern = "");
    ~Search();

    QString get_pattern();

protected:
    void accept();

signals:
    
public slots:
    
private:
    toolbars::TitleBar *_titlebar;
    QLineEdit search_lineEdit;
    QDialogButtonBox search_buttonBox;
    boost::shared_ptr<pv::device::DevInst>  _dev_inst;
};

} // namespace decoder
} // namespace pv

#endif // DSVIEW_PV_SEARCH_H
