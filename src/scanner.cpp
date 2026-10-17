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

#include "scanner.h"
#include "exif.h"

#include <QCollator>
#include <QDataStream>
#include <QDir>
#include <QDirIterator>
#include <QImageReader>
#include <QSaveFile>
#include <QStandardPaths>

#include <algorithm>

namespace {
const quint32 IndexMagic = 0x4c504958; // "LPIX"
const quint32 IndexVersion = 1;
}

QStringList Scanner::imageSuffixes()
{
    static const QStringList suffixes = [] {
        QStringList list;
        for (const QByteArray &format : QImageReader::supportedImageFormats())
            list << QString::fromLatin1(format).toLower();
        // Same formats under other suffixes
        if (list.contains(QLatin1String("jpeg")) || list.contains(QLatin1String("jpg")))
            list << QStringLiteral("jpg") << QStringLiteral("jpeg") << QStringLiteral("jpe") << QStringLiteral("jfif");
        if (list.contains(QLatin1String("tiff")))
            list << QStringLiteral("tif");
        if (list.contains(QLatin1String("heif")) || list.contains(QLatin1String("heic")))
            list << QStringLiteral("heic") << QStringLiteral("heif");
        if (list.contains(QLatin1String("svg")))
            list << QStringLiteral("svgz");
        // Not photos: icons, cursors, documents, Qt's own formats
        for (const char *skip : {"ico", "cur", "icns", "pdf", "xbm", "xpm", "pbm", "pgm", "ppm", "wbmp", "ani", "kra", "ora", "pic", "pcx", "ras", "rgb", "rgba", "sgi", "bw", "pic", "qoi", "xcf", "psd", "psb", "eps", "epsf", "epsi"})
            list.removeAll(QLatin1String(skip));
        list.removeDuplicates();
        return list;
    }();
    return suffixes;
}

bool Scanner::isImageFile(const QString &fileName)
{
    const int dot = fileName.lastIndexOf(QLatin1Char('.'));
    if (dot < 0)
        return false;
    return imageSuffixes().contains(fileName.mid(dot + 1).toLower());
}

QString Scanner::picturesFolder()
{
    // Qt reads ~/.config/user-dirs.dirs, so localized folders (~/Imagens) work
    QString folder = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (folder.isEmpty() || folder == QDir::homePath())
        folder = QDir::homePath() + QStringLiteral("/Pictures");
    return folder;
}

PhotoInfo Scanner::inspect(const QFileInfo &info)
{
    PhotoInfo photo;
    photo.path = info.absoluteFilePath();
    photo.folder = info.absolutePath();
    photo.modified = info.lastModified();
    photo.size = info.size();
    photo.taken = Exif::read(photo.path).dateTaken;
    return photo;
}

QList<PhotoInfo> Scanner::scan(const QString &root, const QHash<QString, PhotoInfo> &known,
                               const std::function<bool()> &cancelled,
                               const std::function<void(const QList<PhotoInfo> &)> &batch, int batchSize)
{
    QList<PhotoInfo> all;
    QList<PhotoInfo> pending;
    // Hidden entries are skipped (no QDir::Hidden), symlinks aren't followed
    QDirIterator it(root, QDir::Files | QDir::NoDotAndDotDot | QDir::Readable,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        if (cancelled && cancelled())
            break;
        it.next();
        const QFileInfo info = it.fileInfo();
        if (!isImageFile(info.fileName()) || info.size() == 0)
            continue;
        // Skip anything below a hidden folder (QDirIterator only hides direct hidden entries)
        const QString relative = info.absoluteFilePath().mid(root.size());
        if (relative.contains(QLatin1String("/.")))
            continue;

        PhotoInfo photo;
        const auto cached = known.constFind(info.absoluteFilePath());
        if (cached != known.constEnd() && cached->size == info.size()
            && cached->modified == info.lastModified()) {
            photo = *cached;
            photo.folder = info.absolutePath();
        } else {
            photo = inspect(info);
        }
        all.append(photo);
        pending.append(photo);
        if (batch && pending.size() >= batchSize) {
            batch(pending);
            pending.clear();
        }
    }
    if (batch && !pending.isEmpty())
        batch(pending);
    return all;
}

QStringList Scanner::folderImages(const QString &folder)
{
    QDir dir(folder);
    QStringList images;
    for (const QFileInfo &info : dir.entryInfoList(QDir::Files | QDir::Readable)) {
        if (isImageFile(info.fileName()))
            images << info.absoluteFilePath();
    }
    // Qt compares plainly in the C locale; natural order needs a real one
    QCollator collator(QLocale().language() == QLocale::C ? QLocale(QLocale::English) : QLocale());
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(images.begin(), images.end(), [&collator](const QString &a, const QString &b) {
        return collator.compare(a, b) < 0;
    });
    return images;
}

QHash<QString, PhotoInfo> Scanner::loadIndex(const QString &file)
{
    QHash<QString, PhotoInfo> index;
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly))
        return index;
    QDataStream in(&f);
    quint32 magic = 0, version = 0;
    qint32 count = 0;
    in >> magic >> version >> count;
    if (magic != IndexMagic || version != IndexVersion || count < 0 || count > 10'000'000)
        return index;
    index.reserve(count);
    for (qint32 i = 0; i < count && in.status() == QDataStream::Ok; ++i) {
        PhotoInfo p;
        in >> p.path >> p.modified >> p.taken >> p.size;
        if (in.status() == QDataStream::Ok)
            index.insert(p.path, p);
    }
    return index;
}

bool Scanner::saveIndex(const QString &file, const QList<PhotoInfo> &photos)
{
    QDir().mkpath(QFileInfo(file).absolutePath());
    QSaveFile f(file);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    QDataStream out(&f);
    out << IndexMagic << IndexVersion << qint32(photos.size());
    for (const PhotoInfo &p : photos)
        out << p.path << p.modified << p.taken << p.size;
    return f.commit();
}
