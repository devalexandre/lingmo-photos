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

// Library grid grouping: date taken with mtime fallback, newest first,
// month headers and rows of N thumbnails.

#include <QTest>

#include "dategroups.h"

namespace {

PhotoInfo photo(const QString &name, const QDateTime &modified, const QDateTime &taken = {})
{
    PhotoInfo p;
    p.path = QStringLiteral("/photos/") + name;
    p.folder = QStringLiteral("/photos");
    p.modified = modified;
    p.taken = taken;
    return p;
}

QDateTime at(int y, int m, int d, int h = 12)
{
    return QDateTime(QDate(y, m, d), QTime(h, 0));
}

} // namespace

class TestDateGroups : public QObject
{
    Q_OBJECT

private slots:
    void monthKey()
    {
        QCOMPARE(DateGroups::monthKey(at(2024, 5, 17)), QStringLiteral("2024-05"));
        QCOMPARE(DateGroups::monthKey(at(1999, 12, 31, 23)), QStringLiteral("1999-12"));
        QCOMPARE(DateGroups::monthKey(QDateTime()), QStringLiteral("0000-00"));
    }

    void monthLabel()
    {
        QCOMPARE(DateGroups::monthLabel(QStringLiteral("2024-05"), QLocale(QLocale::English)), QStringLiteral("May 2024"));
        // Month names are capitalised, also where the locale writes them in lower case
        QCOMPARE(DateGroups::monthLabel(QStringLiteral("2024-03"), QLocale(QLocale::Portuguese, QLocale::Brazil)),
                 QStringLiteral("Março 2024"));
        QCOMPARE(DateGroups::monthLabel(QStringLiteral("0000-00"), QLocale(QLocale::English)), QStringLiteral("Unknown date"));
    }

    void dateTakenWinsOverMtime()
    {
        // Copied yesterday, taken in 2019: it belongs to 2019
        const PhotoInfo p = photo(QStringLiteral("a.jpg"), at(2026, 10, 15), at(2019, 7, 1));
        QCOMPARE(p.date(), at(2019, 7, 1));
        const PhotoInfo q = photo(QStringLiteral("b.png"), at(2026, 10, 15));
        QCOMPARE(q.date(), at(2026, 10, 15));
    }

    void sortAndGroup()
    {
        QList<PhotoInfo> photos = {
            photo(QStringLiteral("old.jpg"), at(2026, 1, 1), at(2023, 5, 2)),
            photo(QStringLiteral("may-b.jpg"), at(2024, 5, 20)),
            photo(QStringLiteral("may-a.jpg"), at(2024, 5, 20)), // same date: by name
            photo(QStringLiteral("june.jpg"), at(2024, 6, 1)),
            photo(QStringLiteral("may-early.jpg"), at(2024, 5, 1)),
            photo(QStringLiteral("dec.jpg"), at(2023, 12, 31, 23)),
        };
        DateGroups::sortNewestFirst(photos);
        QStringList order;
        for (const PhotoInfo &p : photos)
            order << p.fileName();
        QCOMPARE(order, QStringList({QStringLiteral("june.jpg"), QStringLiteral("may-a.jpg"), QStringLiteral("may-b.jpg"),
                                     QStringLiteral("may-early.jpg"), QStringLiteral("dec.jpg"), QStringLiteral("old.jpg")}));

        const QList<DateGroups::Group> groups = DateGroups::group(photos, QLocale(QLocale::English));
        QCOMPARE(groups.size(), 4);
        QCOMPARE(groups[0].label, QStringLiteral("June 2024"));
        QCOMPARE(groups[0].first, 0);
        QCOMPARE(groups[0].count, 1);
        QCOMPARE(groups[1].key, QStringLiteral("2024-05"));
        QCOMPARE(groups[1].first, 1);
        QCOMPARE(groups[1].count, 3);
        QCOMPARE(groups[2].label, QStringLiteral("December 2023"));
        QCOMPARE(groups[3].label, QStringLiteral("May 2023"));
        QCOMPARE(groups[3].first, 5);
    }

    void rows()
    {
        QList<PhotoInfo> photos;
        for (int i = 0; i < 7; ++i)
            photos << photo(QStringLiteral("m%1.jpg").arg(i), at(2025, 3, 20 - i));
        for (int i = 0; i < 2; ++i)
            photos << photo(QStringLiteral("f%1.jpg").arg(i), at(2025, 2, 10 - i));
        DateGroups::sortNewestFirst(photos);
        const QList<DateGroups::Group> groups = DateGroups::group(photos, QLocale(QLocale::English));

        const QList<DateGroups::Row> rows = DateGroups::rows(groups, 3);
        // March: header + 3 + 3 + 1; February: header + 2
        QCOMPARE(rows.size(), 6);
        QVERIFY(rows[0].header);
        QCOMPARE(rows[0].title, QStringLiteral("March 2025"));
        QCOMPARE(rows[0].groupCount, 7);
        QCOMPARE(rows[1].first, 0);
        QCOMPARE(rows[1].count, 3);
        QCOMPARE(rows[2].first, 3);
        QCOMPARE(rows[3].first, 6);
        QCOMPARE(rows[3].count, 1);
        QVERIFY(rows[4].header);
        QCOMPARE(rows[5].first, 7);
        QCOMPARE(rows[5].count, 2);

        // One column, or a bogus value, still covers every photo once
        for (int columns : {1, 0, -4, 100}) {
            int covered = 0;
            for (const DateGroups::Row &r : DateGroups::rows(groups, columns))
                covered += r.header ? 0 : r.count;
            QCOMPARE(covered, 9);
        }
        QVERIFY(DateGroups::rows({}, 4).isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestDateGroups)
#include "tst_dategroups.moc"
