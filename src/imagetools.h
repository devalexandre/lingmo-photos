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

#ifndef IMAGETOOLS_H
#define IMAGETOOLS_H

#include <QObject>
#include <QRectF>
#include <QStringList>
#include <QVariantMap>

class QImage;

// Viewer actions: file details, rotate, crop, wallpaper, clipboard,
// "Open with" and "Show in folder".
class ImageTools : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    // Lingmo Share is installed: "Send to nearby device"
    Q_PROPERTY(bool nearbyShareAvailable READ nearbyShareAvailable CONSTANT)

public:
    explicit ImageTools(QObject *parent = nullptr);

    QString lastError() const { return m_lastError; }

    Q_INVOKABLE QStringList folderImages(const QString &path) const;
    Q_INVOKABLE QVariantMap info(const QString &path) const;
    Q_INVOKABLE bool isAnimated(const QString &path) const;
    Q_INVOKABLE bool exists(const QString &path) const;
    Q_INVOKABLE QString urlFor(const QString &path) const;
    Q_INVOKABLE QString fileName(const QString &path) const;
    // "/home/ana/Imagens" -> "~/Imagens"
    Q_INVOKABLE QString prettyPath(const QString &path) const;

    // Rotation of the displayed image by quarterTurns * 90° clockwise
    Q_INVOKABLE bool canRotateLosslessly(const QString &path) const;
    Q_INVOKABLE bool rotate(const QString &path, int quarterTurns, bool lossless);

    // rect: normalized (0..1) in the displayed image, after quarterTurns.
    // Returns the written file ("" on error); a copy unless replace is set.
    Q_INVOKABLE QString crop(const QString &path, const QRectF &rect, int quarterTurns, bool replace);
    Q_INVOKABLE bool canReplace(const QString &path) const;

    Q_INVOKABLE bool setWallpaper(const QString &path);
    Q_INVOKABLE void copyToClipboard(const QString &path);
    Q_INVOKABLE void showInFolder(const QString &path);
    bool nearbyShareAvailable() const;
    // Asks Lingmo Share where to send the picture (phone, tablet, computer)
    Q_INVOKABLE void sendNearby(const QString &path);

    // [{ id, name, icon }] of the apps that open this type, without us
    Q_INVOKABLE QVariantList openWithApps(const QString &path) const;
    Q_INVOKABLE bool openWith(const QString &path, const QString &appId);

    // "photo (cropped).jpg", "photo (cropped 2).jpg"... next to the original
    static QString copyName(const QString &path, const QString &suffix);
    // Saves keeping the format (and the Exif block of JPEGs) when possible
    static bool saveImage(const QImage &image, const QString &target, const QString &exifSource);

signals:
    void lastErrorChanged();
    // A file was written (rotated, cropped copy): library and thumbnails refresh
    void fileChanged(const QString &path);

private:
    void setError(const QString &error);
    QString m_lastError;
};

#endif
