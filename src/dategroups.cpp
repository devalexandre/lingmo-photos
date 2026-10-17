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

#include "dategroups.h"

#include <QCoreApplication>

#include <algorithm>

QString DateGroups::monthKey(const QDateTime &date)
{
    if (!date.isValid())
        return QStringLiteral("0000-00");
    return date.date().toString(QStringLiteral("yyyy-MM"));
}

QString DateGroups::monthLabel(const QString &key, const QLocale &locale)
{
    const int year = key.left(4).toInt();
    const int month = key.mid(5, 2).toInt();
    if (year <= 0 || month < 1 || month > 12)
        return QCoreApplication::translate("DateGroups", "Unknown date");
    QString name = locale.standaloneMonthName(month, QLocale::LongFormat);
    if (!name.isEmpty())
        name[0] = name[0].toUpper();
    //: Month header in the library grid: %1 month name, %2 year ("May 2024")
    return QCoreApplication::translate("DateGroups", "%1 %2").arg(name).arg(year);
}

void DateGroups::sortNewestFirst(QList<PhotoInfo> &photos)
{
    std::stable_sort(photos.begin(), photos.end(), [](const PhotoInfo &a, const PhotoInfo &b) {
        const QDateTime da = a.date();
        const QDateTime db = b.date();
        if (da != db)
            return da > db;
        return a.path < b.path;
    });
}

QList<DateGroups::Group> DateGroups::group(const QList<PhotoInfo> &photos, const QLocale &locale)
{
    QList<Group> groups;
    for (int i = 0; i < photos.size(); ++i) {
        const QString key = monthKey(photos.at(i).date());
        if (groups.isEmpty() || groups.last().key != key) {
            Group g;
            g.key = key;
            g.label = monthLabel(key, locale);
            g.first = i;
            groups.append(g);
        }
        ++groups.last().count;
    }
    return groups;
}

QList<DateGroups::Row> DateGroups::rows(const QList<Group> &groups, int columns)
{
    columns = std::max(1, columns);
    QList<Row> result;
    for (const Group &g : groups) {
        Row header;
        header.header = true;
        header.title = g.label;
        header.groupCount = g.count;
        header.first = g.first;
        result.append(header);
        for (int i = 0; i < g.count; i += columns) {
            Row row;
            row.first = g.first + i;
            row.count = std::min(columns, g.count - i);
            result.append(row);
        }
    }
    return result;
}
