/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
 * Copyright (C) 2012 Joel Holdsworth <joel@airwebreathe.org.uk>
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


#include <QPixmap>
#include <QApplication>
#include <QTextBrowser>
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QScrollBar>
#include <QTextCodec>

#include "about.h"

namespace pv {
namespace dialogs {

About::About(QWidget *parent) :
    DSDialog(parent, true)
{
    setFixedHeight(360);

    #if defined(__x86_64__) || defined(_M_X64)
        QString arch = "x64";
    #elif defined(__i386) || defined(_M_IX86)
        QString arch = "x86";
    #elif defined(__arm__) || defined(_M_ARM)
        QString arch = "arm";
    #endif

    QString version = tr("<font size=24>DSView %1 (%2)</font><br />")
                      .arg(QApplication::applicationVersion())
                      .arg(arch);

    QString url = tr("Website: <a href=\"%1\" style=\"color:#C0C0C0\">%1</a><br />"
                     "Gitbub: <a href=\"%2\" style=\"color:#C0C0C0\">%2</a><br />"
                     "<br /><br />")
                  .arg(QApplication::organizationDomain())
                  .arg("https://github.com/DreamSourceLab/DSView");

    QString thanks = tr("<font size=16>Special Thanks</font><br />"
                        "<a href=\"%1\" style=\"color:#C0C0C0\">All backers on kickstarter</a><br />"
                        "<a href=\"%2\" style=\"color:#C0C0C0\">All members of Sigrok project</a><br />"
                        "All contributors of all open-source projects</a><br />"
                        "<br /><br />")
                        .arg("https://www.kickstarter.com/projects/dreamsourcelab/dslogic-multifunction-instruments-for-everyone")
                        .arg("http://sigrok.org/");

    QString changlogs = tr("<font size=16>Changelogs</font><br />");
    QDir dir(DS_RES_PATH);
    dir.cdUp();
    QString filename = dir.absolutePath() + "/NEWS";
    QFile news(filename);
    if (news.open(QIODevice::ReadOnly)) {
        QTextCodec *code=QTextCodec::codecForName("UTF-8");
        QTextStream stream(&news);
        stream.setCodec(code);
        QString line;
        while (!stream.atEnd()){
            line = stream.readLine();
            changlogs += line + "<br />";
        }
    }

    QPixmap pix(":/icons/dsl_logo.png");
    QImage logo = pix.toImage();

    QTextBrowser *about = new QTextBrowser(this);
    about->setOpenExternalLinks(true);
    about->setFrameStyle(QFrame::NoFrame);
    QTextCursor cur = about->textCursor();
    cur.insertImage(logo);
    cur.insertHtml("<br /><br /><br />");
    cur.insertHtml(version+url+thanks+changlogs);
    about->moveCursor(QTextCursor::Start);

    QVBoxLayout *xlayout = new QVBoxLayout();
    xlayout->addWidget(about);

    layout()->addLayout(xlayout);
    setTitle(tr("About"));
    setFixedWidth(500);
}

About::~About()
{
}

} // namespace dialogs
} // namespace pv
