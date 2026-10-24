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

#include "imagetools.h"
#include "exif.h"
#include "scanner.h"

#include <QBuffer>
#include <QClipboard>
#include <QCryptographicHash>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImageReader>
#include <QImageWriter>
#include <QLocale>
#include <QMimeData>
#include <QMimeDatabase>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTransform>
#include <QUrl>

#include <KApplicationTrader>
#include <KIO/ApplicationLauncherJob>
#include <KService>

namespace {

QString formatSuffix(const QString &path)
{
    QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == QLatin1String("jpg") || suffix == QLatin1String("jpe") || suffix == QLatin1String("jfif"))
        return QStringLiteral("jpeg");
    if (suffix == QLatin1String("tif"))
        return QStringLiteral("tiff");
    return suffix;
}

bool isJpeg(const QString &path)
{
    return formatSuffix(path) == QLatin1String("jpeg");
}

bool canWrite(const QString &format)
{
    return QImageWriter::supportedImageFormats().contains(format.toLatin1());
}

QImage readDisplayed(const QString &path, int quarterTurns)
{
    QImageReader reader(path);
    reader.setAutoTransform(true);
    QImage image = reader.read();
    if (image.isNull())
        return image;
    const int q = ((quarterTurns % 4) + 4) % 4;
    if (q != 0)
        image = image.transformed(QTransform().rotate(90 * q));
    return image;
}

} // namespace

ImageTools::ImageTools(QObject *parent)
    : QObject(parent)
{
}

void ImageTools::setError(const QString &error)
{
    m_lastError = error;
    emit lastErrorChanged();
}

QStringList ImageTools::folderImages(const QString &path) const
{
    return Scanner::folderImages(QFileInfo(path).absolutePath());
}

bool ImageTools::exists(const QString &path) const
{
    return QFileInfo(path).isFile();
}

QString ImageTools::urlFor(const QString &path) const
{
    return QUrl::fromLocalFile(path).toString();
}

QString ImageTools::fileName(const QString &path) const
{
    return QFileInfo(path).fileName();
}

QString ImageTools::prettyPath(const QString &path) const
{
    const QString home = QDir::homePath();
    if (path == home)
        return QStringLiteral("~");
    if (path.startsWith(home + QLatin1Char('/')))
        return QStringLiteral("~") + path.mid(home.size());
    return path;
}

bool ImageTools::isAnimated(const QString &path) const
{
    QImageReader reader(path);
    return reader.supportsAnimation() && reader.imageCount() != 1;
}

QVariantMap ImageTools::info(const QString &path) const
{
    QVariantMap map;
    const QFileInfo file(path);
    if (!file.isFile())
        return map;
    const QLocale locale;
    QImageReader reader(path);
    QSize size = reader.size();
    if (reader.transformation() & QImageIOHandler::TransformationRotate90)
        size.transpose();
    const ExifInfo exif = Exif::read(path);
    const QMimeType mime = QMimeDatabase().mimeTypeForFile(file);

    map.insert(QStringLiteral("name"), file.fileName());
    map.insert(QStringLiteral("path"), file.absoluteFilePath());
    map.insert(QStringLiteral("folder"), QDir::toNativeSeparators(file.absolutePath()));
    map.insert(QStringLiteral("width"), size.width());
    map.insert(QStringLiteral("height"), size.height());
    map.insert(QStringLiteral("dimensions"), size.isValid()
               ? tr("%1 × %2 pixels").arg(size.width()).arg(size.height()) : QString());
    if (size.isValid())
        map.insert(QStringLiteral("megapixels"),
                   locale.toString(double(size.width()) * size.height() / 1e6, 'f', 1) + QStringLiteral(" MP"));
    map.insert(QStringLiteral("fileSize"), locale.formattedDataSize(file.size()));
    map.insert(QStringLiteral("format"), mime.comment());
    map.insert(QStringLiteral("mimeType"), mime.name());
    //: Date format of the info panel (QLocale/QDateTime syntax)
    const QString dateFormat = tr("d MMMM yyyy, HH:mm");
    map.insert(QStringLiteral("modified"), locale.toString(file.lastModified(), dateFormat));
    if (exif.dateTaken.isValid())
        map.insert(QStringLiteral("taken"), locale.toString(exif.dateTaken, dateFormat));
    map.insert(QStringLiteral("camera"), exif.camera());
    map.insert(QStringLiteral("lens"), exif.lens);

    QStringList exposure;
    if (exif.exposureTime > 0) {
        exposure << (exif.exposureTime < 0.5
                         ? QStringLiteral("1/%1 s").arg(qRound(1.0 / exif.exposureTime))
                         : locale.toString(exif.exposureTime, 'g', 2) + QStringLiteral(" s"));
    }
    if (exif.fNumber > 0)
        exposure << QStringLiteral("ƒ/") + locale.toString(exif.fNumber, 'g', 2);
    if (exif.iso > 0)
        exposure << QStringLiteral("ISO %1").arg(exif.iso);
    if (exif.focalLength > 0)
        exposure << locale.toString(exif.focalLength, 'g', 4) + QStringLiteral(" mm");
    map.insert(QStringLiteral("exposure"), exposure.join(QStringLiteral(" · ")));
    map.insert(QStringLiteral("animated"), isAnimated(path));
    return map;
}

bool ImageTools::canRotateLosslessly(const QString &path) const
{
    // Qt applies the Exif orientation of JPEG and TIFF files
    const QString format = formatSuffix(path);
    if (format != QLatin1String("jpeg") && format != QLatin1String("tiff"))
        return false;
    return Exif::read(path).orientationOffset >= 0 && QFileInfo(path).isWritable();
}

bool ImageTools::canReplace(const QString &path) const
{
    return QFileInfo(path).isWritable() && canWrite(formatSuffix(path)) && !isAnimated(path);
}

QString ImageTools::copyName(const QString &path, const QString &suffix)
{
    const QFileInfo info(path);
    const QString base = info.completeBaseName();
    const QString ext = info.suffix().isEmpty() ? QString() : QLatin1Char('.') + info.suffix();
    QString candidate;
    for (int n = 1;; ++n) {
        const QString tag = n == 1 ? suffix : QStringLiteral("%1 %2").arg(suffix).arg(n);
        candidate = info.absoluteDir().absoluteFilePath(QStringLiteral("%1 (%2)%3").arg(base, tag, ext));
        if (!QFileInfo::exists(candidate))
            return candidate;
    }
}

bool ImageTools::saveImage(const QImage &image, const QString &target, const QString &exifSource)
{
    const QString format = formatSuffix(target);
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    QImageWriter writer(&buffer, format.toLatin1());
    if (format == QLatin1String("jpeg") || format == QLatin1String("webp"))
        writer.setQuality(92);
    if (!writer.write(image))
        return false;
    buffer.close();
    if (format == QLatin1String("jpeg") && !exifSource.isEmpty() && isJpeg(exifSource))
        bytes = Exif::withExifSegment(bytes, Exif::jpegExifSegment(exifSource));

    QSaveFile file(target);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size())
        return false;
    return file.commit();
}

bool ImageTools::rotate(const QString &path, int quarterTurns, bool lossless)
{
    const int q = ((quarterTurns % 4) + 4) % 4;
    if (q == 0)
        return true;
    if (lossless) {
        const ExifInfo exif = Exif::read(path);
        if (exif.orientationOffset < 0 || !Exif::writeOrientation(path, Exif::rotatedOrientation(exif.orientation, q))) {
            setError(tr("Couldn't update the orientation of this photo."));
            return false;
        }
        emit fileChanged(path);
        return true;
    }
    if (!canReplace(path)) {
        setError(tr("This format can't be saved here."));
        return false;
    }
    const QImage image = readDisplayed(path, q);
    if (image.isNull() || !saveImage(image, path, path)) {
        setError(tr("Couldn't save the rotated photo."));
        return false;
    }
    emit fileChanged(path);
    return true;
}

QString ImageTools::crop(const QString &path, const QRectF &rect, int quarterTurns, bool replace)
{
    const QImage image = readDisplayed(path, quarterTurns);
    if (image.isNull()) {
        setError(tr("Couldn't open the photo."));
        return {};
    }
    const QRectF normalized = rect.normalized().intersected(QRectF(0, 0, 1, 1));
    const QRect area = QRect(qRound(normalized.x() * image.width()), qRound(normalized.y() * image.height()),
                             qRound(normalized.width() * image.width()), qRound(normalized.height() * image.height()))
                           .intersected(image.rect());
    if (area.width() < 1 || area.height() < 1) {
        setError(tr("The selected area is empty."));
        return {};
    }
    const QImage cropped = image.copy(area);

    QString target;
    if (replace && canReplace(path)) {
        target = path;
    } else {
        //: Added to the file name of a cropped copy: "beach (cropped).jpg"
        target = copyName(path, tr("cropped"));
        if (!canWrite(formatSuffix(path)) || isAnimated(path))
            target = QFileInfo(target).absolutePath() + QLatin1Char('/') + QFileInfo(target).completeBaseName() + QStringLiteral(".png");
    }
    if (!saveImage(cropped, target, path)) {
        setError(tr("Couldn't save the cropped photo."));
        return {};
    }
    emit fileChanged(target);
    return target;
}

bool ImageTools::setWallpaper(const QString &path)
{
    // A private copy: the desktop keeps its picture if the photo is moved or deleted
    const QString folder = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
        + QStringLiteral("/lingmo-photos/wallpapers");
    QDir().mkpath(folder);
    const QString tag = QString::fromLatin1(
        QCryptographicHash::hash(QFileInfo(path).absoluteFilePath().toUtf8(), QCryptographicHash::Md5).toHex().left(8));
    const QString target = folder + QLatin1Char('/') + tag + QLatin1Char('-') + QFileInfo(path).fileName();
    QFile::remove(target);
    if (!QFile::copy(path, target)) {
        setError(tr("Couldn't copy the photo."));
        return false;
    }

    // Same call Lingmo Settings makes (lingmo-settings-daemon, ThemeManager)
    QDBusInterface theme(QStringLiteral("com.lingmo.Settings"), QStringLiteral("/Theme"),
                         QStringLiteral("com.lingmo.Theme"), QDBusConnection::sessionBus());
    if (!theme.isValid()) {
        setError(tr("The Lingmo settings service isn't running."));
        return false;
    }
    theme.call(QStringLiteral("setBackgroundType"), 0);
    const QDBusMessage reply = theme.call(QStringLiteral("setWallpaper"), target);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        setError(reply.errorMessage());
        return false;
    }
    // Only the current wallpaper is kept
    for (const QFileInfo &old : QDir(folder).entryInfoList(QDir::Files)) {
        if (old.absoluteFilePath() != target)
            QFile::remove(old.absoluteFilePath());
    }
    return true;
}

void ImageTools::copyToClipboard(const QString &path)
{
    auto *mime = new QMimeData;
    const QUrl url = QUrl::fromLocalFile(path);
    mime->setUrls({url});
    // Pasting in a file manager copies the file; in an editor, the picture
    mime->setData(QStringLiteral("x-special/gnome-copied-files"), "copy\n" + url.toEncoded());
    QImageReader reader(path);
    reader.setAutoTransform(true);
    const QImage image = reader.read();
    if (!image.isNull())
        mime->setImageData(image);
    QGuiApplication::clipboard()->setMimeData(mime);
}

bool ImageTools::nearbyShareAvailable() const
{
    return !QStandardPaths::findExecutable(QStringLiteral("lingmo-share")).isEmpty();
}

void ImageTools::sendNearby(const QString &path)
{
    QProcess::startDetached(QStringLiteral("lingmo-share"), {QStringLiteral("send"), QStringLiteral("--pick"), path});
}

void ImageTools::showInFolder(const QString &path)
{
    QDBusMessage call = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.FileManager1"), QStringLiteral("/org/freedesktop/FileManager1"),
        QStringLiteral("org.freedesktop.FileManager1"), QStringLiteral("ShowItems"));
    call << QStringList{QUrl::fromLocalFile(path).toString()} << QString();
    const QDBusMessage reply = QDBusConnection::sessionBus().call(call, QDBus::Block, 3000);
    if (reply.type() != QDBusMessage::ReplyMessage)
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
}

QVariantList ImageTools::openWithApps(const QString &path) const
{
    QVariantList apps;
    const QString mime = QMimeDatabase().mimeTypeForFile(path).name();
    const KService::List services = KApplicationTrader::queryByMimeType(mime);
    for (const KService::Ptr &service : services) {
        if (service->desktopEntryName() == QLatin1String("lingmo-photos") || service->noDisplay())
            continue;
        apps.append(QVariantMap{
            {QStringLiteral("id"), service->storageId()},
            {QStringLiteral("name"), service->name()},
            {QStringLiteral("icon"), service->icon()},
        });
    }
    return apps;
}

bool ImageTools::openWith(const QString &path, const QString &appId)
{
    const KService::Ptr service = KService::serviceByStorageId(appId);
    if (!service) {
        setError(tr("That application is no longer installed."));
        return false;
    }
    auto *job = new KIO::ApplicationLauncherJob(service);
    job->setUrls({QUrl::fromLocalFile(path)});
    job->start();
    return true;
}
