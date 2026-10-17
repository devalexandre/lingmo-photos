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

// Trash and undo: files go to the freedesktop.org trash of a temporary
// HOME (with their .trashinfo), and "Undo" brings them back without ever
// overwriting a file that took the name in the meantime.

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

#include "testutil.h"
#include "trash.h"

class TestTrash : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        m_home = m_dir.filePath(QStringLiteral("home"));
        TestUtil::isolateHome(m_home);
        m_trash = m_home + QStringLiteral("/.local/share/Trash");
    }

    void infoPath()
    {
        QCOMPARE(Trash::infoPathFor(QStringLiteral("/h/.local/share/Trash/files/a.jpg")),
                 QStringLiteral("/h/.local/share/Trash/info/a.jpg.trashinfo"));
    }

    void trashAndUndo()
    {
        const QString a = m_home + QStringLiteral("/Pictures/a.jpg");
        const QString b = m_home + QStringLiteral("/Pictures/sub/b.png");
        QVERIFY(TestUtil::writeFile(a, "AAAA"));
        QVERIFY(TestUtil::writeFile(b, "BBBB"));

        Trash trash;
        QSignalSpy trashed(&trash, &Trash::trashed);
        QSignalSpy restored(&trash, &Trash::restored);

        const int batch = trash.moveToTrash({a, b});
        QVERIFY(batch > 0);
        QVERIFY(trash.canUndo(batch));
        QVERIFY(trash.lastFailed().isEmpty());
        QCOMPARE(trashed.count(), 1);
        QCOMPARE(trashed.first().first().toStringList(), QStringList({a, b}));
        QVERIFY(!QFileInfo::exists(a));
        QVERIFY(!QFileInfo::exists(b));
        QVERIFY(QFileInfo::exists(m_trash + QStringLiteral("/files/a.jpg")));
        QVERIFY(QFileInfo::exists(m_trash + QStringLiteral("/info/a.jpg.trashinfo")));
        QFile info(m_trash + QStringLiteral("/info/b.png.trashinfo"));
        QVERIFY(info.open(QIODevice::ReadOnly));
        QVERIFY(info.readAll().contains("Path=" + QUrl::fromLocalFile(b).path(QUrl::FullyEncoded).toUtf8()));
        info.close();

        const QStringList back = trash.undo(batch);
        QCOMPARE(back, QStringList({a, b}));
        QCOMPARE(restored.count(), 1);
        QVERIFY(QFileInfo::exists(a));
        QVERIFY(QFileInfo::exists(b));
        QFile content(b);
        QVERIFY(content.open(QIODevice::ReadOnly));
        QCOMPARE(content.readAll(), QByteArray("BBBB"));
        // Trash is clean again
        QVERIFY(!QFileInfo::exists(m_trash + QStringLiteral("/files/a.jpg")));
        QVERIFY(!QFileInfo::exists(m_trash + QStringLiteral("/info/a.jpg.trashinfo")));
        QVERIFY(!QFileInfo::exists(m_trash + QStringLiteral("/info/b.png.trashinfo")));

        // A batch is undone once
        QVERIFY(!trash.canUndo(batch));
        QVERIFY(trash.undo(batch).isEmpty());
        QCOMPARE(restored.count(), 1);
    }

    void undoNeverOverwrites()
    {
        const QString c = m_home + QStringLiteral("/Pictures/c.jpg");
        QVERIFY(TestUtil::writeFile(c, "old"));
        Trash trash;
        const int batch = trash.moveToTrash({c});
        QVERIFY(batch > 0);
        // A new file with the same name appears
        QVERIFY(TestUtil::writeFile(c, "new"));
        QVERIFY(trash.undo(batch).isEmpty());
        QFile f(c);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QCOMPARE(f.readAll(), QByteArray("new"));
        // The old one stays in the trash, with its info
        QVERIFY(QFileInfo::exists(m_trash + QStringLiteral("/files/c.jpg")));
        QVERIFY(QFileInfo::exists(m_trash + QStringLiteral("/info/c.jpg.trashinfo")));
    }

    void sameNameTwice()
    {
        // Two "d.jpg" from different folders: both kept, both restored to their place
        const QString d1 = m_home + QStringLiteral("/Pictures/x/d.jpg");
        const QString d2 = m_home + QStringLiteral("/Pictures/y/d.jpg");
        QVERIFY(TestUtil::writeFile(d1, "1"));
        QVERIFY(TestUtil::writeFile(d2, "2"));
        Trash trash;
        const int first = trash.moveToTrash({d1});
        const int second = trash.moveToTrash({d2});
        QVERIFY(first > 0 && second > 0 && first != second);
        QCOMPARE(trash.undo(first), QStringList{d1});
        QCOMPARE(trash.undo(second), QStringList{d2});
        QFile f(d2);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QCOMPARE(f.readAll(), QByteArray("2"));
    }

    void failures()
    {
        Trash trash;
        const QString missing = m_home + QStringLiteral("/Pictures/missing.jpg");
        QCOMPARE(trash.moveToTrash({missing}), 0);
        QCOMPARE(trash.lastFailed(), QStringList{missing});
        QCOMPARE(trash.moveToTrash({}), 0);
        QVERIFY(trash.undo(12345).isEmpty());
    }

private:
    QTemporaryDir m_dir;
    QString m_home;
    QString m_trash;
};

QTEST_GUILESS_MAIN(TestTrash)
#include "tst_trash.moc"
