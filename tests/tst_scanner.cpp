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

// Folder scanning (recursive, hidden entries skipped, EXIF dates, index
// cache), the XDG Pictures folder, folder order for the viewer, and the
// library model on top of it (worker thread, filters, favourites, selection).

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "photolibrary.h"
#include "scanner.h"
#include "testutil.h"

class TestScanner : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        m_home = m_dir.filePath(QStringLiteral("home"));
        TestUtil::isolateHome(m_home);
        m_root = m_home + QStringLiteral("/Imagens");

        const QImage image = TestUtil::testImage(32, 24);
        const QByteArray png = TestUtil::encode(image, "png");
        const QDateTime old(QDate(2021, 3, 10), QTime(10, 0));
        QVERIFY(TestUtil::writeFile(m_root + QStringLiteral("/camera.jpg"), TestUtil::jpegWithExif(image,
                                    TestUtil::Tiff().build(QStringLiteral("Fuji"), QStringLiteral("X-T5"), 1,
                                                           QStringLiteral("2020:05:01 08:00:00"))),
                                    QDateTime(QDate(2026, 1, 1), QTime(9, 0))));
        QVERIFY(TestUtil::writeFile(m_root + QStringLiteral("/Trip/beach.png"), png, old));
        QVERIFY(TestUtil::writeFile(m_root + QStringLiteral("/Trip/Day 2/sunset.PNG"), png, old.addDays(1)));
        // Not photos, or not meant to be seen
        QVERIFY(TestUtil::writeFile(m_root + QStringLiteral("/.hidden/secret.png"), png));
        QVERIFY(TestUtil::writeFile(m_root + QStringLiteral("/Trip/.cache/x/thumb.png"), png));
        QVERIFY(TestUtil::writeFile(m_root + QStringLiteral("/Trip/.dotfile.png"), png));
        QVERIFY(TestUtil::writeFile(m_root + QStringLiteral("/notes.txt"), "hello"));
        QVERIFY(TestUtil::writeFile(m_root + QStringLiteral("/empty.jpg"), QByteArray()));
    }

    void suffixes()
    {
        QVERIFY(Scanner::isImageFile(QStringLiteral("a.jpg")));
        QVERIFY(Scanner::isImageFile(QStringLiteral("a.JPEG")));
        QVERIFY(Scanner::isImageFile(QStringLiteral("a.png")));
        QVERIFY(Scanner::isImageFile(QStringLiteral("a.gif")));
        QVERIFY(!Scanner::isImageFile(QStringLiteral("a.txt")));
        QVERIFY(!Scanner::isImageFile(QStringLiteral("jpg")));
        QVERIFY(!Scanner::isImageFile(QStringLiteral("icon.ico"))); // icons aren't photos
    }

    void picturesFolderFollowsXdg()
    {
        QVERIFY(TestUtil::writeFile(m_home + QStringLiteral("/.config/user-dirs.dirs"),
                                    "XDG_PICTURES_DIR=\"$HOME/Imagens\"\n"));
        QCOMPARE(Scanner::picturesFolder(), m_root);
    }

    void scan()
    {
        int batches = 0;
        const QList<PhotoInfo> photos = Scanner::scan(m_root, {}, {}, [&](const QList<PhotoInfo> &) { ++batches; }, 2);
        QStringList names;
        for (const PhotoInfo &p : photos)
            names << QDir(m_root).relativeFilePath(p.path);
        names.sort();
        QCOMPARE(names, QStringList({QStringLiteral("Trip/Day 2/sunset.PNG"), QStringLiteral("Trip/beach.png"),
                                     QStringLiteral("camera.jpg")}));
        QCOMPARE(batches, 2); // 2 + 1

        for (const PhotoInfo &p : photos) {
            if (p.fileName() == QLatin1String("camera.jpg")) {
                QCOMPARE(p.taken, QDateTime(QDate(2020, 5, 1), QTime(8, 0)));
                QCOMPARE(p.date(), p.taken);
                QCOMPARE(p.folder, m_root);
            } else if (p.fileName() == QLatin1String("beach.png")) {
                QVERIFY(!p.taken.isValid());
                QCOMPARE(p.date(), QDateTime(QDate(2021, 3, 10), QTime(10, 0)));
                QCOMPARE(p.folder, m_root + QStringLiteral("/Trip"));
            }
        }

        // Cancelled right away: nothing
        QVERIFY(Scanner::scan(m_root, {}, [] { return true; }).isEmpty());
        QVERIFY(Scanner::scan(m_root + QStringLiteral("/missing")).isEmpty());
    }

    void indexCache()
    {
        const QList<PhotoInfo> photos = Scanner::scan(m_root);
        const QString file = m_dir.filePath(QStringLiteral("index.dat"));
        QVERIFY(Scanner::saveIndex(file, photos));
        QHash<QString, PhotoInfo> index = Scanner::loadIndex(file);
        QCOMPARE(index.size(), 3);
        const QString camera = m_root + QStringLiteral("/camera.jpg");
        QCOMPARE(index.value(camera).taken, QDateTime(QDate(2020, 5, 1), QTime(8, 0)));

        // Unchanged files take their date from the index (no EXIF read)...
        index[camera].taken = QDateTime(QDate(2001, 1, 1), QTime(0, 0));
        auto takenOf = [&](const QList<PhotoInfo> &list) {
            for (const PhotoInfo &p : list) {
                if (p.path == camera)
                    return p.taken;
            }
            return QDateTime();
        };
        QCOMPARE(takenOf(Scanner::scan(m_root, index)), QDateTime(QDate(2001, 1, 1), QTime(0, 0)));
        // ...changed ones are read again
        index[camera].size += 1;
        QCOMPARE(takenOf(Scanner::scan(m_root, index)), QDateTime(QDate(2020, 5, 1), QTime(8, 0)));

        QVERIFY(TestUtil::writeFile(file, "garbage"));
        QVERIFY(Scanner::loadIndex(file).isEmpty());
    }

    void folderOrder()
    {
        const QString dir = m_dir.filePath(QStringLiteral("seq"));
        const QByteArray png = TestUtil::encode(TestUtil::testImage(4, 4), "png");
        for (const char *name : {"IMG_10.png", "IMG_2.png", "img_1.png", "notes.txt", "IMG_3.webp"})
            QVERIFY(TestUtil::writeFile(dir + QLatin1Char('/') + QLatin1String(name), png));
        QStringList names;
        for (const QString &p : Scanner::folderImages(dir))
            names << QFileInfo(p).fileName();
        QStringList expected = {QStringLiteral("img_1.png"), QStringLiteral("IMG_2.png")};
        if (Scanner::isImageFile(QStringLiteral("x.webp")))
            expected << QStringLiteral("IMG_3.webp");
        expected << QStringLiteral("IMG_10.png");
        QCOMPARE(names, expected);
    }

    void library()
    {
        PhotoLibrary library(m_root);
        QCOMPARE(library.rootFolder(), m_root);
        QSignalSpy scanning(&library, &PhotoLibrary::scanningChanged);
        library.start();
        QVERIFY(library.scanning());
        QTRY_VERIFY(library.loaded());
        QVERIFY(!library.scanning());
        QCOMPARE(library.totalCount(), 3);
        // Newest first: Trip/Day 2 (2021-03-11), beach (2021-03-10), camera (taken 2020)
        QCOMPARE(library.fileName(0), QStringLiteral("sunset.PNG"));
        QCOMPARE(library.fileName(2), QStringLiteral("camera.jpg"));

        // Rows: two months, header + photos each
        auto *rows = qobject_cast<RowsModel *>(library.rows());
        library.setColumns(4);
        QCOMPARE(rows->rowCount(), 4);
        QCOMPARE(rows->data(rows->index(0), RowsModel::TitleRole).toString(),
                 DateGroups::monthLabel(QStringLiteral("2021-03")));
        QCOMPARE(rows->data(rows->index(1), RowsModel::CountRole).toInt(), 2);
        QCOMPARE(library.rowForPhoto(2), 3);

        // Folders page
        auto *folders = qobject_cast<FoldersModel *>(library.folders());
        QCOMPARE(folders->rowCount(), 3);
        QCOMPARE(folders->data(folders->index(0), FoldersModel::PathRole).toString(), m_root); // root first
        QCOMPARE(folders->data(folders->index(1), FoldersModel::RelativeRole).toString(), QStringLiteral("Trip"));
        QCOMPARE(folders->data(folders->index(2), FoldersModel::RelativeRole).toString(), QStringLiteral("Trip/Day 2"));

        // Search by name, by year, by month name
        library.setSearchText(QStringLiteral("beach"));
        QCOMPARE(library.count(), 1);
        library.setSearchText(QStringLiteral("2020"));
        QCOMPARE(library.visiblePaths(), QStringList{m_root + QStringLiteral("/camera.jpg")});
        library.setSearchText(DateGroups::monthLabel(QStringLiteral("2021-03")).section(QLatin1Char(' '), 0, 0).toLower());
        QCOMPARE(library.count(), 2);
        library.setSearchText(QStringLiteral("nothing-like-this"));
        QCOMPARE(library.count(), 0);
        library.setSearchText({});
        QCOMPARE(library.count(), 3);

        // Folder filter (not recursive)
        library.setFolderFilter(m_root + QStringLiteral("/Trip"));
        QCOMPARE(library.visiblePaths(), QStringList{m_root + QStringLiteral("/Trip/beach.png")});
        library.setFolderFilter({});

        // Favourites persist and filter
        library.setFavorite(m_root + QStringLiteral("/camera.jpg"), true);
        QCOMPARE(library.favoritesCount(), 1);
        QCOMPARE(PhotoLibrary::loadFavorites(), QStringList{m_root + QStringLiteral("/camera.jpg")});
        library.setFavoritesOnly(true);
        QCOMPARE(library.count(), 1);
        library.setFavoritesOnly(false);

        // Selection, ranges, removal after trashing
        library.selectRange(2, 0);
        QCOMPARE(library.selectedCount(), 3);
        library.toggleSelected(1);
        QCOMPARE(library.selectedCount(), 2);
        QVERIFY(!library.isSelected(1));
        QCOMPARE(library.selectedPaths().first(), library.path(0)); // grid order
        QVERIFY(!library.allSelectedFavorite());
        library.removePaths({library.path(0)});
        QCOMPARE(library.totalCount(), 2);
        QCOMPARE(library.selectedCount(), 1);
        library.clearSelection();
        QCOMPARE(library.selectedCount(), 0);

        // Restored from the trash: it's back (files outside the root are ignored)
        library.addPaths({m_root + QStringLiteral("/Trip/Day 2/sunset.PNG"), m_dir.filePath(QStringLiteral("seq/IMG_2.png"))});
        QCOMPARE(library.totalCount(), 3);

        // The index was written for the next start
        QVERIFY(QFileInfo::exists(m_home + QStringLiteral("/.cache/lingmo-photos/index.dat")));
        QVERIFY(scanning.count() >= 2);
    }

    void emptyLibrary()
    {
        PhotoLibrary library(m_dir.filePath(QStringLiteral("nothing-here")));
        library.start();
        QTRY_VERIFY(library.loaded());
        QCOMPARE(library.totalCount(), 0);
        QCOMPARE(library.rowForPhoto(0), -1);
        QVERIFY(library.path(0).isEmpty());
    }

private:
    QTemporaryDir m_dir;
    QString m_home;
    QString m_root;
};

QTEST_GUILESS_MAIN(TestScanner)
#include "tst_scanner.moc"
