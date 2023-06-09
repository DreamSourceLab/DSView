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


#include "about.h"

#include <QPixmap>
#include <QApplication>
#include <QTextBrowser>
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QScrollBar>
  
#include "../config/appconfig.h"
#include "../dsvdef.h"
#include "../utility/encoding.h"
#include "../ui/langresource.h"

namespace pv {
namespace dialogs {

About::About(QWidget *parent) :
    DSDialog(parent, true)
{
    setFixedHeight(600);
    setFixedWidth(800);

    #if defined(__x86_64__) || defined(_M_X64)
        QString arch = "x64";
    #elif defined(__i386) || defined(_M_IX86)
        QString arch = "x86";
    #elif defined(__arm__) || defined(_M_ARM)
        QString arch = "arm";
    #elif defined(__aarch64__)
        QString arch = "arm64";
    #else
        QString arch = "other";
    #endif

    QString version = tr("<font size=24>DSView %1 (%2)</font><br />")
                      .arg(QApplication::applicationVersion())
                      .arg(arch);

    QString site_url = QApplication::organizationDomain();
    if (site_url.startsWith("http") == false){
        site_url = "https://" + site_url;
    }

    QString url = tr("Website: <a href=\"%1\" style=\"color:#C0C0C0\">%1</a><br />"
                     "Github: <a href=\"%2\" style=\"color:#C0C0C0\">%2</a><br />"
                     "Copyright: <label href=\"#\" style=\"color:#C0C0C0\">%3</label><br />"
                     "<br /><br />")
                  .arg(site_url)
                  .arg("https://github.com/DreamSourceLab/DSView")
                  .arg(tr("© DreamSourceLab. All rights reserved."));

    QString thanks = tr("<font size=16>Special Thanks</font><br />"
                        "<a href=\"%1\" style=\"color:#C0C0C0\">All backers on kickstarter</a><br />"
                        "<a href=\"%2\" style=\"color:#C0C0C0\">All members of Sigrok project</a><br />"
                        "All contributors of all open-source projects</a><br />"
                        "<br /><br />")
                        .arg("https://www.kickstarter.com/projects/dreamsourcelab/dslogic-multifunction-instruments-for-everyone")
                        .arg("http://sigrok.org/");

    QString changlogs = tr("<font size=16>Changelogs</font><br />");

    QDir dir(GetAppDataDir());
    AppConfig &app = AppConfig::Instance(); 
    int lan = app.frameOptions.language;

    QString filename = dir.absolutePath() + "/NEWS" + QString::number(lan);
    QFile news(filename);
    if (news.open(QIODevice::ReadOnly)) {
   
        QTextStream stream(&news);
        encoding::set_utf8(stream);

        QString line;
        while (!stream.atEnd()){
            line = stream.readLine();
            changlogs += line + "<br />";
        }
    }
    news.close();    

    QPixmap pix(":/icons/dsl_logo.svg");
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
    setTitle(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_ABOUT), "About"));
}

About::~About()
{
}

} // namespace dialogs
} // namespace pv
