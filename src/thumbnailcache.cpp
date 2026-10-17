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

#include "thumbnailcache.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QImageWriter>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrl>

namespace {

const QString FailFolder = QStringLiteral("fail/lingmo-photos-1.0");

bool makePrivateDir(const QString &dir)
{
    if (QFileInfo::exists(dir))
        return true;
    if (!QDir().mkpath(dir))
        return false;
    // The spec wants the thumbnail folders readable by the owner only
    QFile::setPermissions(dir, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    QFile::setPermissions(QFileInfo(dir).absolutePath(),
                          QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    return true;
}

bool savePng(const QImage &image, const QString &target)
{
    if (!makePrivateDir(QFileInfo(target).absolutePath()))
        return false;
    // Written to a temporary file and renamed: other readers never see half a PNG
    QSaveFile file(target);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    QImageWriter writer(&file, "png");
    if (!writer.write(image)) {
        file.cancelWriting();
        return false;
    }
    if (!file.commit())
        return false;
    QFile::setPermissions(target, QFile::ReadOwner | QFile::WriteOwner);
    return true;
}

QString mtimeOf(const QFileInfo &info)
{
    return QString::number(info.lastModified().toSecsSinceEpoch());
}

} // namespace

QString Thumbnails::flavorFor(int size)
{
    if (size <= 128)
        return QStringLiteral("normal");
    if (size <= 256)
        return QStringLiteral("large");
    if (size <= 512)
        return QStringLiteral("x-large");
    return QStringLiteral("xx-large");
}

int Thumbnails::flavorSize(const QString &flavor)
{
    if (flavor == QLatin1String("normal"))
        return 128;
    if (flavor == QLatin1String("large"))
        return 256;
    if (flavor == QLatin1String("x-large"))
        return 512;
    return 1024;
}

QString Thumbnails::cacheRoot()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation)
        + QStringLiteral("/thumbnails");
}

QString Thumbnails::uriFor(const QString &path)
{
    // file:///home/me/My%20Photo.jpg — RFC 2396 escaping, as the spec asks
    return QString::fromLatin1(QUrl::fromLocalFile(QFileInfo(path).absoluteFilePath()).toEncoded());
}

QString Thumbnails::fileNameFor(const QString &path)
{
    const QByteArray md5 = QCryptographicHash::hash(uriFor(path).toUtf8(), QCryptographicHash::Md5);
    return QString::fromLatin1(md5.toHex()) + QStringLiteral(".png");
}

QString Thumbnails::pathFor(const QString &path, int size)
{
    return cacheRoot() + QLatin1Char('/') + flavorFor(size) + QLatin1Char('/') + fileNameFor(path);
}

QString Thumbnails::failPathFor(const QString &path)
{
    return cacheRoot() + QLatin1Char('/') + FailFolder + QLatin1Char('/') + fileNameFor(path);
}

namespace {
// QImageReader::text() can't return keys with ':' in them (it parses
// "key: value" pairs), so the whole thumbnail is read; they're small.
bool matches(const QImage &thumb, const QString &source)
{
    return !thumb.isNull()
        && thumb.text(QStringLiteral("Thumb::MTime")) == mtimeOf(QFileInfo(source))
        && thumb.text(QStringLiteral("Thumb::URI")) == Thumbnails::uriFor(source);
}
}

bool Thumbnails::isValid(const QString &thumbPath, const QString &source)
{
    return QFileInfo::exists(thumbPath) && matches(QImage(thumbPath, "png"), source);
}

QImage Thumbnails::load(const QString &source, int size)
{
    const QFileInfo info(source);
    if (!info.isFile())
        return {};
    const int flavor = flavorSize(flavorFor(size));
    const QString cached = pathFor(source, size);
    // Files inside the thumbnail folders never get thumbnails of their own
    const bool insideCache = info.absoluteFilePath().startsWith(cacheRoot() + QLatin1Char('/'));

    if (!insideCache && QFileInfo::exists(cached)) {
        const QImage image(cached, "png");
        if (matches(image, source))
            return image;
    }
    const QString failed = failPathFor(source);
    if (!insideCache && isValid(failed, source))
        return {};

    QImageReader reader(source);
    reader.setAutoTransform(true);
    reader.setQuality(80);
    // The box is square, so the EXIF rotation doesn't change the fitted size.
    // Decoders like JPEG scale while decoding: far faster than a full read.
    const QSize original = reader.size();
    if (original.isValid() && (original.width() > flavor || original.height() > flavor)
        && reader.supportsOption(QImageIOHandler::ScaledSize))
        reader.setScaledSize(original.scaled(flavor, flavor, Qt::KeepAspectRatio).expandedTo(QSize(1, 1)));
    QImage image = reader.read();
    if (image.isNull()) {
        if (!insideCache) {
            QImage marker(1, 1, QImage::Format_ARGB32);
            marker.fill(Qt::transparent);
            marker.setText(QStringLiteral("Thumb::URI"), uriFor(source));
            marker.setText(QStringLiteral("Thumb::MTime"), mtimeOf(info));
            savePng(marker, failed);
        }
        return {};
    }
    if (image.width() > flavor || image.height() > flavor)
        image = image.scaled(flavor, flavor, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    if (image.format() != QImage::Format_ARGB32 && image.format() != QImage::Format_RGB32)
        image = image.convertToFormat(image.hasAlphaChannel() ? QImage::Format_ARGB32 : QImage::Format_RGB32);

    if (!insideCache) {
        QImage stored = image;
        stored.setText(QStringLiteral("Thumb::URI"), uriFor(source));
        stored.setText(QStringLiteral("Thumb::MTime"), mtimeOf(info));
        stored.setText(QStringLiteral("Thumb::Size"), QString::number(info.size()));
        stored.setText(QStringLiteral("Software"), QStringLiteral("Lingmo Photos"));
        savePng(stored, cached);
    }
    return image;
}
