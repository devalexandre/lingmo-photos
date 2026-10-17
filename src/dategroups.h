/*
 * Copyright (C) 2026 LingmoOS Team.
 *
 * Author:     devalexandre <alexandre@dev2learn.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef DATEGROUPS_H
#define DATEGROUPS_H

#include <QDateTime>
#include <QList>
#include <QLocale>
#include <QString>

#include "scanner.h"

// Grouping of the library grid by month
namespace DateGroups
{
struct Group
{
    QString key;      // "2024-05"
    QString label;    // "May 2024"
    int first = 0;    // index of the first photo in the sorted list
    int count = 0;
};

// One row of the grid: either a month header or up to `columns` photos
struct Row
{
    bool header = false;
    QString title;
    int groupCount = 0; // header: photos in the month
    int first = 0;      // photos row: first photo index
    int count = 0;      // photos row: number of photos
};

QString monthKey(const QDateTime &date);
QString monthLabel(const QString &key, const QLocale &locale = QLocale());

// Newest first; same date -> by file name, so the order is stable
void sortNewestFirst(QList<PhotoInfo> &photos);

// photos must be sorted newest first
QList<Group> group(const QList<PhotoInfo> &photos, const QLocale &locale = QLocale());
QList<Row> rows(const QList<Group> &groups, int columns);
}

#endif
