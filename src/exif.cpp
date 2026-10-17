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

#include "exif.h"

#include <QFile>
#include <QtEndian>

#include <cstring>

namespace {

class TiffReader
{
public:
    TiffReader(const char *data, qint64 size)
        : m_data(reinterpret_cast<const uchar *>(data)), m_size(size)
    {
    }

    bool ok(qint64 off, qint64 len) const { return off >= 0 && len >= 0 && off + len <= m_size; }

    quint16 u16(qint64 off) const
    {
        if (!ok(off, 2))
            return 0;
        return m_big ? qFromBigEndian<quint16>(m_data + off) : qFromLittleEndian<quint16>(m_data + off);
    }

    quint32 u32(qint64 off) const
    {
        if (!ok(off, 4))
            return 0;
        return m_big ? qFromBigEndian<quint32>(m_data + off) : qFromLittleEndian<quint32>(m_data + off);
    }

    bool m_big = false;
    const uchar *m_data;
    qint64 m_size;
};

struct Entry
{
    quint16 tag = 0;
    quint16 type = 0;
    quint32 count = 0;
    qint64 valueOffset = 0; // where the value lives (inline or pointed to)
};

int typeSize(quint16 type)
{
    switch (type) {
    case 1: case 2: case 6: case 7: return 1;   // BYTE ASCII SBYTE UNDEFINED
    case 3: case 8: return 2;                   // SHORT SSHORT
    case 4: case 9: case 11: return 4;          // LONG SLONG FLOAT
    case 5: case 10: case 12: return 8;         // RATIONAL SRATIONAL DOUBLE
    default: return 0;
    }
}

QList<Entry> readIfd(const TiffReader &r, qint64 ifd)
{
    QList<Entry> entries;
    if (!r.ok(ifd, 2))
        return entries;
    const int n = r.u16(ifd);
    if (n <= 0 || n > 1000 || !r.ok(ifd + 2, qint64(n) * 12))
        return entries;
    for (int i = 0; i < n; ++i) {
        const qint64 e = ifd + 2 + qint64(i) * 12;
        Entry entry;
        entry.tag = r.u16(e);
        entry.type = r.u16(e + 2);
        entry.count = r.u32(e + 4);
        const qint64 bytes = qint64(typeSize(entry.type)) * entry.count;
        entry.valueOffset = bytes <= 4 ? e + 8 : qint64(r.u32(e + 8));
        if (bytes > 0 && r.ok(entry.valueOffset, bytes))
            entries.append(entry);
    }
    return entries;
}

QByteArray ascii(const TiffReader &r, const Entry &e)
{
    if (e.type != 2 && e.type != 7)
        return {};
    QByteArray value(reinterpret_cast<const char *>(r.m_data + e.valueOffset), int(e.count));
    const int nul = value.indexOf('\0');
    if (nul >= 0)
        value.truncate(nul);
    return value.trimmed();
}

double rational(const TiffReader &r, const Entry &e)
{
    if (e.type == 5 || e.type == 10) {
        const double num = e.type == 10 ? double(qint32(r.u32(e.valueOffset))) : double(r.u32(e.valueOffset));
        const double den = e.type == 10 ? double(qint32(r.u32(e.valueOffset + 4))) : double(r.u32(e.valueOffset + 4));
        return den != 0 ? num / den : 0;
    }
    if (e.type == 3)
        return r.u16(e.valueOffset);
    if (e.type == 4)
        return r.u32(e.valueOffset);
    return 0;
}

int integer(const TiffReader &r, const Entry &e)
{
    if (e.type == 3)
        return r.u16(e.valueOffset);
    if (e.type == 4 || e.type == 9)
        return int(r.u32(e.valueOffset));
    return int(rational(r, e));
}

// Locates the TIFF block in a mapped file; returns its offset or -1
qint64 findTiff(const uchar *d, qint64 size)
{
    if (size < 12)
        return -1;
    // JPEG: walk the markers up to the image data
    if (d[0] == 0xFF && d[1] == 0xD8) {
        qint64 pos = 2;
        while (pos + 4 <= size) {
            if (d[pos] != 0xFF)
                return -1;
            const uchar marker = d[pos + 1];
            if (marker == 0xFF) { // fill byte
                ++pos;
                continue;
            }
            if (marker == 0xD9 || marker == 0xDA) // EOI / SOS
                return -1;
            if (marker >= 0xD0 && marker <= 0xD7) {
                pos += 2;
                continue;
            }
            const qint64 len = qFromBigEndian<quint16>(d + pos + 2);
            if (len < 2)
                return -1;
            if (marker == 0xE1 && len >= 8 && pos + 4 + 6 <= size
                && std::memcmp(d + pos + 4, "Exif\0\0", 6) == 0)
                return pos + 10;
            pos += 2 + len;
        }
        return -1;
    }
    // TIFF itself (and most camera raw formats)
    if ((d[0] == 'I' && d[1] == 'I' && d[2] == 42 && d[3] == 0)
        || (d[0] == 'M' && d[1] == 'M' && d[2] == 0 && d[3] == 42))
        return 0;
    // PNG: eXIf chunk before the image data
    static const uchar pngSig[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    if (std::memcmp(d, pngSig, 8) == 0) {
        qint64 pos = 8;
        while (pos + 8 <= size) {
            const qint64 len = qFromBigEndian<quint32>(d + pos);
            const char *type = reinterpret_cast<const char *>(d + pos + 4);
            if (std::memcmp(type, "eXIf", 4) == 0)
                return pos + 8 + len <= size ? pos + 8 : -1;
            if (std::memcmp(type, "IEND", 4) == 0)
                return -1;
            pos += 12 + len;
        }
        return -1;
    }
    // WebP: RIFF container with an EXIF chunk
    if (std::memcmp(d, "RIFF", 4) == 0 && std::memcmp(d + 8, "WEBP", 4) == 0) {
        qint64 pos = 12;
        while (pos + 8 <= size) {
            const qint64 len = qFromLittleEndian<quint32>(d + pos + 4);
            if (std::memcmp(d + pos, "EXIF", 4) == 0) {
                qint64 start = pos + 8;
                if (start + 6 <= size && std::memcmp(d + start, "Exif\0\0", 6) == 0)
                    start += 6;
                return start;
            }
            pos += 8 + len + (len & 1);
        }
    }
    return -1;
}

} // namespace

QString ExifInfo::camera() const
{
    // "Canon Canon EOS R6" -> "Canon EOS R6"
    if (!make.isEmpty() && model.startsWith(make, Qt::CaseInsensitive))
        return model;
    return QStringList({make, model}).join(QLatin1Char(' ')).trimmed();
}

QDateTime Exif::parseDateTime(const QByteArray &value)
{
    if (value.size() < 19)
        return {};
    // Some writers use '-' instead of ':' in the date part
    const QByteArray v = value.left(19);
    const QDate date(v.mid(0, 4).toInt(), v.mid(5, 2).toInt(), v.mid(8, 2).toInt());
    const QTime time(v.mid(11, 2).toInt(), v.mid(14, 2).toInt(), v.mid(17, 2).toInt());
    if (!date.isValid() || date.year() < 1900 || !time.isValid())
        return {};
    return QDateTime(date, time);
}

ExifInfo Exif::parseTiff(const char *data, qint64 size, qint64 base)
{
    ExifInfo info;
    TiffReader r(data, size);
    if (size < 8)
        return info;
    if (data[0] == 'M' && data[1] == 'M')
        r.m_big = true;
    else if (!(data[0] == 'I' && data[1] == 'I'))
        return info;
    if (r.u16(2) != 42)
        return info;
    info.bigEndian = r.m_big;

    QByteArray dateTime, original, digitized;
    qint64 exifIfd = -1;
    const QList<Entry> ifd0 = readIfd(r, r.u32(4));
    for (const Entry &e : ifd0) {
        switch (e.tag) {
        case 0x010F: info.make = QString::fromUtf8(ascii(r, e)); break;
        case 0x0110: info.model = QString::fromUtf8(ascii(r, e)); break;
        case 0x0112:
            if (e.type == 3 && e.valueOffset + 2 <= size) {
                info.orientation = r.u16(e.valueOffset);
                info.orientationOffset = base + e.valueOffset;
            }
            break;
        case 0x0132: dateTime = ascii(r, e); break;
        case 0x8769: exifIfd = integer(r, e); break;
        default: break;
        }
    }
    if (exifIfd > 0) {
        for (const Entry &e : readIfd(r, exifIfd)) {
            switch (e.tag) {
            case 0x9003: original = ascii(r, e); break;
            case 0x9004: digitized = ascii(r, e); break;
            case 0x829A: info.exposureTime = rational(r, e); break;
            case 0x829D: info.fNumber = rational(r, e); break;
            case 0x8827: info.iso = integer(r, e); break;
            case 0x920A: info.focalLength = rational(r, e); break;
            case 0xA434: info.lens = QString::fromUtf8(ascii(r, e)); break;
            default: break;
            }
        }
    }
    for (const QByteArray &candidate : {original, digitized, dateTime}) {
        info.dateTaken = parseDateTime(candidate);
        if (info.dateTaken.isValid())
            break;
    }
    if (info.orientation < 1 || info.orientation > 8) {
        info.orientation = 0;
        info.orientationOffset = -1;
    }
    info.valid = !ifd0.isEmpty();
    return info;
}

ExifInfo Exif::read(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    const qint64 size = file.size();
    if (size < 12)
        return {};
    qint64 available = size;
    const uchar *d = file.map(0, size);
    QByteArray buffer;
    if (!d) {
        buffer = file.read(4 * 1024 * 1024);
        d = reinterpret_cast<const uchar *>(buffer.constData());
        available = buffer.size();
    }
    const qint64 tiff = findTiff(d, available);
    if (tiff < 0)
        return {};
    return parseTiff(reinterpret_cast<const char *>(d + tiff), available - tiff, tiff);
}

int Exif::rotatedOrientation(int orientation, int quarterTurns)
{
    // Displayed image = rotate(r * 90° cw) after mirror(m), per orientation value
    static const int mirror[9] = {0, 0, 1, 0, 1, 1, 0, 1, 0};
    static const int rot[9] = {0, 0, 0, 2, 2, 3, 1, 1, 3};
    if (orientation < 1 || orientation > 8)
        orientation = 1;
    const int m = mirror[orientation];
    const int r = (((rot[orientation] + quarterTurns) % 4) + 4) % 4;
    for (int o = 1; o <= 8; ++o) {
        if (mirror[o] == m && rot[o] == r)
            return o;
    }
    return 1;
}

bool Exif::writeOrientation(const QString &path, int orientation)
{
    if (orientation < 1 || orientation > 8)
        return false;
    const ExifInfo info = read(path);
    if (info.orientationOffset < 0)
        return false;
    QFile file(path);
    if (!file.open(QIODevice::ReadWrite) || !file.seek(info.orientationOffset))
        return false;
    uchar bytes[2];
    if (info.bigEndian)
        qToBigEndian<quint16>(quint16(orientation), bytes);
    else
        qToLittleEndian<quint16>(quint16(orientation), bytes);
    return file.write(reinterpret_cast<const char *>(bytes), 2) == 2;
}

QByteArray Exif::jpegExifSegment(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    // The Exif APP1 segment is at most 64 KiB and sits near the start
    const QByteArray head = file.read(512 * 1024);
    const uchar *d = reinterpret_cast<const uchar *>(head.constData());
    if (head.size() < 4 || d[0] != 0xFF || d[1] != 0xD8)
        return {};
    const qint64 tiff = findTiff(d, head.size());
    if (tiff < 10)
        return {};
    const qint64 start = tiff - 10; // FF E1 len(2) "Exif\0\0"
    const qint64 length = qFromBigEndian<quint16>(d + start + 2);
    if (start + 2 + length > head.size())
        return {};
    QByteArray segment = head.mid(start, 2 + length);
    const ExifInfo info = parseTiff(segment.constData() + 10, segment.size() - 10, 0);
    if (info.orientationOffset >= 0) {
        uchar *o = reinterpret_cast<uchar *>(segment.data()) + 10 + info.orientationOffset;
        if (info.bigEndian)
            qToBigEndian<quint16>(1, o);
        else
            qToLittleEndian<quint16>(1, o);
    }
    return segment;
}

QByteArray Exif::withExifSegment(const QByteArray &jpeg, const QByteArray &segment)
{
    if (segment.isEmpty() || jpeg.size() < 2 || uchar(jpeg[0]) != 0xFF || uchar(jpeg[1]) != 0xD8)
        return jpeg;
    return jpeg.left(2) + segment + jpeg.mid(2);
}
