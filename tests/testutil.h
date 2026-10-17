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

// Helpers shared by the tests: a temporary HOME with its own XDG folders,
// and JPEG/PNG files with a hand-made Exif block.

#ifndef TESTUTIL_H
#define TESTUTIL_H

#include <QBuffer>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QImageWriter>
#include <QList>
#include <QString>
#include <QtEndian>

namespace TestUtil
{

// Must run before anything touches QStandardPaths
inline void isolateHome(const QString &home)
{
    QDir().mkpath(home);
    qputenv("HOME", home.toLocal8Bit());
    qputenv("XDG_CACHE_HOME", (home + QStringLiteral("/.cache")).toLocal8Bit());
    qputenv("XDG_DATA_HOME", (home + QStringLiteral("/.local/share")).toLocal8Bit());
    qputenv("XDG_CONFIG_HOME", (home + QStringLiteral("/.config")).toLocal8Bit());
    qunsetenv("XDG_PICTURES_DIR");
}

struct Entry
{
    quint16 tag;
    quint16 type;
    quint32 count;
    QByteArray data; // already in the block's byte order
};

class Tiff
{
public:
    explicit Tiff(bool bigEndian = false) : be(bigEndian) {}

    QByteArray u16(quint16 v) const
    {
        QByteArray b(2, 0);
        if (be)
            qToBigEndian<quint16>(v, b.data());
        else
            qToLittleEndian<quint16>(v, b.data());
        return b;
    }
    QByteArray u32(quint32 v) const
    {
        QByteArray b(4, 0);
        if (be)
            qToBigEndian<quint32>(v, b.data());
        else
            qToLittleEndian<quint32>(v, b.data());
        return b;
    }
    static QByteArray ascii(const QString &s) { return s.toLatin1() + '\0'; }

    QByteArray ifd(const QList<Entry> &entries, quint32 at) const
    {
        const quint32 dataStart = at + 2 + 12 * entries.size() + 4;
        QByteArray head = u16(quint16(entries.size()));
        QByteArray tail;
        for (const Entry &e : entries) {
            head += u16(e.tag) + u16(e.type) + u32(e.count);
            if (e.data.size() <= 4) {
                head += e.data + QByteArray(4 - e.data.size(), '\0');
            } else {
                head += u32(dataStart + tail.size());
                tail += e.data;
                if (tail.size() % 2)
                    tail += '\0';
            }
        }
        head += u32(0);
        return head + tail;
    }

    // IFD0 (make, model, orientation) + Exif IFD (date taken, ISO, f-number)
    QByteArray build(const QString &make, const QString &model, int orientation,
                     const QString &dateTaken, int iso = 200) const
    {
        QList<Entry> exif;
        if (!dateTaken.isEmpty())
            exif.append({0x9003, 2, quint32(dateTaken.size() + 1), ascii(dateTaken)});
        exif.append({0x8827, 3, 1, u16(quint16(iso))});
        exif.append({0x829D, 5, 1, u32(28) + u32(10)});

        auto ifd0For = [&](quint32 exifOffset) {
            QList<Entry> e;
            if (!make.isEmpty())
                e.append({0x010F, 2, quint32(make.size() + 1), ascii(make)});
            if (!model.isEmpty())
                e.append({0x0110, 2, quint32(model.size() + 1), ascii(model)});
            if (orientation > 0)
                e.append({0x0112, 3, 1, u16(quint16(orientation))});
            e.append({0x8769, 4, 1, u32(exifOffset)});
            return ifd(e, 8);
        };
        const QByteArray header = (be ? QByteArray("MM") : QByteArray("II")) + u16(42) + u32(8);
        const quint32 exifOffset = 8 + ifd0For(0).size();
        return header + ifd0For(exifOffset) + ifd(exif, exifOffset);
    }

    bool be;
};

inline QImage testImage(int w, int h)
{
    QImage image(w, h, QImage::Format_RGB32);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x)
            image.setPixel(x, y, qRgb(x * 255 / w, y * 255 / h, 128));
    }
    return image;
}

inline QByteArray encode(const QImage &image, const char *format)
{
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    QImageWriter writer(&buffer, format);
    writer.write(image);
    return bytes;
}

inline QByteArray jpegWithExif(const QImage &image, const QByteArray &tiff)
{
    const QByteArray jpeg = encode(image, "jpeg");
    QByteArray segment("\xFF\xE1", 2);
    QByteArray len(2, 0);
    qToBigEndian<quint16>(quint16(2 + 6 + tiff.size()), len.data());
    segment += len + QByteArray("Exif\0\0", 6) + tiff;
    return jpeg.left(2) + segment + jpeg.mid(2);
}

inline quint32 crc32(const QByteArray &data)
{
    quint32 crc = 0xFFFFFFFF;
    for (char c : data) {
        crc ^= uchar(c);
        for (int k = 0; k < 8; ++k)
            crc = (crc >> 1) ^ (0xEDB88320 & (0 - (crc & 1)));
    }
    return ~crc;
}

// PNG with an eXIf chunk right after IHDR
inline QByteArray pngWithExif(const QImage &image, const QByteArray &tiff)
{
    const QByteArray png = encode(image, "png");
    const int afterIhdr = 8 + 4 + 4 + 13 + 4;
    QByteArray chunk(4, 0);
    qToBigEndian<quint32>(quint32(tiff.size()), chunk.data());
    const QByteArray body = QByteArray("eXIf") + tiff;
    QByteArray crc(4, 0);
    qToBigEndian<quint32>(crc32(body), crc.data());
    chunk += body + crc;
    return png.left(afterIhdr) + chunk + png.mid(afterIhdr);
}

inline bool writeFile(const QString &path, const QByteArray &bytes, const QDateTime &mtime = {})
{
    QDir().mkpath(path.left(path.lastIndexOf(QLatin1Char('/'))));
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly) || f.write(bytes) != bytes.size())
        return false;
    f.flush(); // or closing would write again and bump the mtime
    if (mtime.isValid())
        f.setFileTime(mtime, QFileDevice::FileModificationTime);
    return true;
}

} // namespace TestUtil

#endif
