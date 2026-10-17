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

// Exif reader: date taken, camera, orientation in JPEG (both byte orders)
// and PNG, in-place orientation updates, and keeping the Exif block when
// a JPEG is saved again.

#include <QImageReader>
#include <QTemporaryDir>
#include <QTest>

#include "exif.h"
#include "testutil.h"

class TestExif : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
    }

    void readsJpeg_data()
    {
        QTest::addColumn<bool>("bigEndian");
        QTest::newRow("little endian") << false;
        QTest::newRow("big endian") << true;
    }

    void readsJpeg()
    {
        QFETCH(bool, bigEndian);
        const QString path = m_dir.filePath(QStringLiteral("camera-%1.jpg").arg(bigEndian));
        const TestUtil::Tiff tiff(bigEndian);
        QVERIFY(TestUtil::writeFile(path, TestUtil::jpegWithExif(TestUtil::testImage(64, 32),
                                    tiff.build(QStringLiteral("Canon"), QStringLiteral("Canon EOS R6"), 6,
                                               QStringLiteral("2023:07:14 18:30:05"), 400))));

        const ExifInfo info = Exif::read(path);
        QVERIFY(info.valid);
        QCOMPARE(info.bigEndian, bigEndian);
        QCOMPARE(info.dateTaken, QDateTime(QDate(2023, 7, 14), QTime(18, 30, 5)));
        QCOMPARE(info.camera(), QStringLiteral("Canon EOS R6")); // make not repeated
        QCOMPARE(info.orientation, 6);
        QVERIFY(info.orientationOffset > 0);
        QCOMPARE(info.iso, 400);
        QCOMPARE(info.fNumber, 2.8);

        // Qt honours the same tag
        QImageReader reader(path);
        QCOMPARE(reader.transformation(), QImageIOHandler::TransformationRotate90);
    }

    void readsPng()
    {
        const QString path = m_dir.filePath(QStringLiteral("screenshot.png"));
        QVERIFY(TestUtil::writeFile(path, TestUtil::pngWithExif(TestUtil::testImage(20, 10),
                                    TestUtil::Tiff().build(QStringLiteral("Google"), QStringLiteral("Pixel 8"), 1,
                                                           QStringLiteral("2024:12:24 20:00:00")))));
        const ExifInfo info = Exif::read(path);
        QCOMPARE(info.dateTaken, QDateTime(QDate(2024, 12, 24), QTime(20, 0)));
        QCOMPARE(info.camera(), QStringLiteral("Google Pixel 8"));
        QVERIFY(!QImageReader(path).read().isNull()); // still a valid PNG
    }

    void noExif()
    {
        const QString path = m_dir.filePath(QStringLiteral("plain.jpg"));
        QVERIFY(TestUtil::writeFile(path, TestUtil::encode(TestUtil::testImage(8, 8), "jpeg")));
        const ExifInfo info = Exif::read(path);
        QVERIFY(!info.valid);
        QVERIFY(!info.dateTaken.isValid());
        QCOMPARE(info.orientationOffset, qint64(-1));
        // Garbage and truncated files don't crash
        QVERIFY(TestUtil::writeFile(m_dir.filePath(QStringLiteral("junk.jpg")), QByteArray("\xFF\xD8\xFF\xE1\xFF\xFF" "Exif", 10)));
        QVERIFY(!Exif::read(m_dir.filePath(QStringLiteral("junk.jpg"))).valid);
        QVERIFY(!Exif::read(m_dir.filePath(QStringLiteral("missing.jpg"))).valid);
    }

    void dateParsing()
    {
        QCOMPARE(Exif::parseDateTime("2021:02:03 04:05:06"), QDateTime(QDate(2021, 2, 3), QTime(4, 5, 6)));
        QCOMPARE(Exif::parseDateTime("2021-02-03 04:05:06"), QDateTime(QDate(2021, 2, 3), QTime(4, 5, 6)));
        QVERIFY(!Exif::parseDateTime("0000:00:00 00:00:00").isValid()); // "unknown" in cameras
        QVERIFY(!Exif::parseDateTime("    :  :     :  :  ").isValid());
        QVERIFY(!Exif::parseDateTime("2021").isValid());
    }

    void rotatedOrientation()
    {
        QCOMPARE(Exif::rotatedOrientation(1, 1), 6);
        QCOMPARE(Exif::rotatedOrientation(6, 1), 3);
        QCOMPARE(Exif::rotatedOrientation(3, 1), 8);
        QCOMPARE(Exif::rotatedOrientation(8, 1), 1);
        QCOMPARE(Exif::rotatedOrientation(1, -1), 8);
        QCOMPARE(Exif::rotatedOrientation(1, 2), 3);
        QCOMPARE(Exif::rotatedOrientation(0, 1), 6); // missing = normal
        // Mirrored ones keep the mirror
        QCOMPARE(Exif::rotatedOrientation(2, 1), 7);
        QCOMPARE(Exif::rotatedOrientation(7, 1), 4);
        QCOMPARE(Exif::rotatedOrientation(4, 1), 5);
        QCOMPARE(Exif::rotatedOrientation(5, 1), 2);
        for (int o = 1; o <= 8; ++o)
            QCOMPARE(Exif::rotatedOrientation(Exif::rotatedOrientation(o, 1), -1), o);
    }

    void writeOrientationInPlace()
    {
        const QString path = m_dir.filePath(QStringLiteral("turn.jpg"));
        QVERIFY(TestUtil::writeFile(path, TestUtil::jpegWithExif(TestUtil::testImage(40, 20),
                                    TestUtil::Tiff(true).build(QStringLiteral("X"), QStringLiteral("Y"), 1,
                                                               QStringLiteral("2020:01:01 00:00:00")))));
        const qint64 size = QFileInfo(path).size();
        QVERIFY(Exif::writeOrientation(path, 6));
        QCOMPARE(QFileInfo(path).size(), size); // only two bytes changed
        QCOMPARE(Exif::read(path).orientation, 6);
        QImageReader reader(path);
        reader.setAutoTransform(true);
        QCOMPARE(reader.read().size(), QSize(20, 40));

        // No tag to patch
        const QString plain = m_dir.filePath(QStringLiteral("plain2.jpg"));
        QVERIFY(TestUtil::writeFile(plain, TestUtil::encode(TestUtil::testImage(8, 8), "jpeg")));
        QVERIFY(!Exif::writeOrientation(plain, 6));
        QVERIFY(!Exif::writeOrientation(path, 9));
    }

    void keepsExifWhenReencoding()
    {
        const QString path = m_dir.filePath(QStringLiteral("source.jpg"));
        QVERIFY(TestUtil::writeFile(path, TestUtil::jpegWithExif(TestUtil::testImage(40, 20),
                                    TestUtil::Tiff().build(QStringLiteral("Nikon"), QStringLiteral("Z6"), 8,
                                                           QStringLiteral("2019:09:09 09:09:09")))));
        const QByteArray segment = Exif::jpegExifSegment(path);
        QVERIFY(segment.startsWith("\xFF\xE1"));

        const QString copy = m_dir.filePath(QStringLiteral("copy.jpg"));
        QVERIFY(TestUtil::writeFile(copy, Exif::withExifSegment(TestUtil::encode(TestUtil::testImage(20, 40), "jpeg"), segment)));
        const ExifInfo info = Exif::read(copy);
        QCOMPARE(info.dateTaken, QDateTime(QDate(2019, 9, 9), QTime(9, 9, 9)));
        QCOMPARE(info.camera(), QStringLiteral("Nikon Z6"));
        QCOMPARE(info.orientation, 1); // pixels are already upright
        QCOMPARE(QImageReader(copy).read().size(), QSize(20, 40));
    }

private:
    QTemporaryDir m_dir;
};

QTEST_GUILESS_MAIN(TestExif)
#include "tst_exif.moc"
