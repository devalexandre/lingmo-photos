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

// Thumbnail cache, as the freedesktop.org Thumbnail Managing Standard says:
// md5 of the file URI, flavor folders, Thumb::URI/Thumb::MTime, private
// permissions, regeneration when the file changes, failure markers.

#include <QCryptographicHash>
#include <QImageReader>
#include <QTemporaryDir>
#include <QTest>

#include "testutil.h"
#include "thumbnailcache.h"

class TestThumbnails : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        TestUtil::isolateHome(m_dir.filePath(QStringLiteral("home")));
        m_cache = m_dir.filePath(QStringLiteral("home/.cache/thumbnails"));
    }

    void uriAndName()
    {
        QCOMPARE(Thumbnails::uriFor(QStringLiteral("/home/ana/Imagens/Praia 2024/foto #1.jpg")),
                 QStringLiteral("file:///home/ana/Imagens/Praia%202024/foto%20%231.jpg"));
        QCOMPARE(Thumbnails::uriFor(QStringLiteral("/tmp/ação.png")), QStringLiteral("file:///tmp/a%C3%A7%C3%A3o.png"));

        // The spec's own example
        QCOMPARE(Thumbnails::fileNameFor(QStringLiteral("/home/jens/photos/me.png")),
                 QStringLiteral("c6ee772d9e49320e97ec29a7eb5b1697.png"));
        const QByteArray md5 = QCryptographicHash::hash("file:///tmp/x.jpg", QCryptographicHash::Md5).toHex();
        QCOMPARE(Thumbnails::fileNameFor(QStringLiteral("/tmp/x.jpg")), QString::fromLatin1(md5) + QStringLiteral(".png"));
    }

    void flavors()
    {
        QCOMPARE(Thumbnails::flavorFor(64), QStringLiteral("normal"));
        QCOMPARE(Thumbnails::flavorFor(128), QStringLiteral("normal"));
        QCOMPARE(Thumbnails::flavorFor(200), QStringLiteral("large"));
        QCOMPARE(Thumbnails::flavorFor(512), QStringLiteral("x-large"));
        QCOMPARE(Thumbnails::flavorFor(4000), QStringLiteral("xx-large"));
        QCOMPARE(Thumbnails::cacheRoot(), m_cache);
        QCOMPARE(Thumbnails::pathFor(QStringLiteral("/tmp/x.jpg"), 256),
                 m_cache + QStringLiteral("/large/") + Thumbnails::fileNameFor(QStringLiteral("/tmp/x.jpg")));
    }

    void createsAndReuses()
    {
        const QString photo = m_dir.filePath(QStringLiteral("photos/wide.png"));
        QVERIFY(TestUtil::writeFile(photo, TestUtil::encode(TestUtil::testImage(1000, 500), "png"),
                                    QDateTime(QDate(2024, 1, 2), QTime(3, 4, 5))));

        const QImage thumb = Thumbnails::load(photo, 200);
        QCOMPARE(thumb.size(), QSize(256, 128)); // large flavor, aspect kept

        const QString stored = Thumbnails::pathFor(photo, 256);
        QVERIFY(QFileInfo::exists(stored));
        QVERIFY(Thumbnails::isValid(stored, photo));
        const QImage reader(stored);
        QCOMPARE(reader.text(QStringLiteral("Thumb::URI")), Thumbnails::uriFor(photo));
        QCOMPARE(reader.text(QStringLiteral("Thumb::MTime")),
                 QString::number(QDateTime(QDate(2024, 1, 2), QTime(3, 4, 5)).toSecsSinceEpoch()));
        QCOMPARE(QFileInfo(stored).permissions() & (QFile::ReadGroup | QFile::ReadOther), QFile::Permissions());
        QCOMPARE(QFileInfo(m_cache + QStringLiteral("/large")).permissions() & QFile::ReadOther, QFile::Permissions());

        // Small sizes use the normal flavor; small images aren't enlarged
        QCOMPARE(Thumbnails::load(photo, 100).size(), QSize(128, 64));
        const QString tiny = m_dir.filePath(QStringLiteral("photos/tiny.png"));
        QVERIFY(TestUtil::writeFile(tiny, TestUtil::encode(TestUtil::testImage(40, 30), "png")));
        QCOMPARE(Thumbnails::load(tiny, 256).size(), QSize(40, 30));

        // The cached file is used: replace it with a marker image and see it come back
        QImage marker(256, 128, QImage::Format_RGB32);
        marker.fill(Qt::red);
        marker.setText(QStringLiteral("Thumb::URI"), Thumbnails::uriFor(photo));
        marker.setText(QStringLiteral("Thumb::MTime"), reader.text(QStringLiteral("Thumb::MTime")));
        QVERIFY(marker.save(stored, "PNG"));
        QCOMPARE(Thumbnails::load(photo, 256).pixel(10, 10), qRgb(255, 0, 0));

        // A newer version of the photo makes it stale and it's made again
        QFile f(photo);
        QVERIFY(f.open(QIODevice::ReadWrite));
        QVERIFY(f.setFileTime(QDateTime(QDate(2025, 6, 7), QTime(8, 9, 10)), QFileDevice::FileModificationTime));
        f.close();
        QVERIFY(!Thumbnails::isValid(stored, photo));
        QVERIFY(Thumbnails::load(photo, 256).pixel(10, 10) != qRgb(255, 0, 0));
        QVERIFY(Thumbnails::isValid(stored, photo));
    }

    void followsExifOrientation()
    {
        const QString photo = m_dir.filePath(QStringLiteral("photos/portrait.jpg"));
        QVERIFY(TestUtil::writeFile(photo, TestUtil::jpegWithExif(TestUtil::testImage(600, 300),
                                    TestUtil::Tiff().build(QStringLiteral("A"), QStringLiteral("B"), 6, {}))));
        QCOMPARE(Thumbnails::load(photo, 256).size(), QSize(128, 256));
    }

    void failureMarker()
    {
        const QString broken = m_dir.filePath(QStringLiteral("photos/broken.jpg"));
        QVERIFY(TestUtil::writeFile(broken, QByteArray("this is not a jpeg")));
        QVERIFY(Thumbnails::load(broken, 256).isNull());
        const QString fail = Thumbnails::failPathFor(broken);
        QVERIFY(fail.startsWith(m_cache + QStringLiteral("/fail/lingmo-photos-1.0/")));
        QVERIFY(Thumbnails::isValid(fail, broken));
        QVERIFY(!QFileInfo::exists(Thumbnails::pathFor(broken, 256)));
        QVERIFY(Thumbnails::load(m_dir.filePath(QStringLiteral("photos/none.jpg")), 256).isNull());
    }

    void noThumbnailsOfThumbnails()
    {
        const QString stored = Thumbnails::pathFor(m_dir.filePath(QStringLiteral("photos/wide.png")), 256);
        QVERIFY(QFileInfo::exists(stored));
        QVERIFY(!Thumbnails::load(stored, 128).isNull());
        QVERIFY(!QFileInfo::exists(Thumbnails::pathFor(stored, 128)));
    }

private:
    QTemporaryDir m_dir;
    QString m_cache;
};

QTEST_GUILESS_MAIN(TestThumbnails)
#include "tst_thumbnails.moc"
