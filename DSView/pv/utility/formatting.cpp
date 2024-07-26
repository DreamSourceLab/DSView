/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
 * Copyright (C) 2024 DreamSourceLab <support@dreamsourcelab.com>
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

#include "formatting.h"
#include <QDateTime>
#include <QString>
#include <stdlib.h>

QString Formatting::DateTimeToString(QDateTime tm, TimeStrigFormatType format)
{
    int year = tm.date().year();
    int month = tm.date().month();
    int day = tm.date().day();
    int hour = tm.time().hour();
    int minute = tm.time().minute();
    int second = tm.time().second();

    char buffer[20];

    switch (format)
    {
    case TimeStrigFormatType::TIME_STR_FORMAT_SHORT2: //"yyMMdd-hhmmss"
        sprintf(buffer, "%02d%02d%02d-%02d%02d%02d", year%100, month, day, hour, minute, second);
        break;
    default: //yyyy-MM-dd hh:mm:ss
        sprintf(buffer, "%04d-%02d-%02d %02d:%02d:%02d", year, month, day, hour, minute, second);   
        break;
    }

    return QString(buffer);
}