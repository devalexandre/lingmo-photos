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

#ifndef SCANNER_H
#define SCANNER_H

#include <QDateTime>
#include <QFileInfo>
#include <QHash>
#include <QList>
#include <QMetaType>
#include <QString>
#include <QStringList>

#include <functional>

struct PhotoInfo
{
    QString path;
    QString folder;
    QDateTime modified;
    QDateTime taken;     // from EXIF, invalid when unknown
    qint64 size = 0;

    // Date used for sorting and grouping: date taken, else file mtime
    QDateTime date() const { return taken.isValid() ? taken : modified; }
    QString fileName() const { return path.mid(path.lastIndexOf(QLatin1Char('/')) + 1); }
};
Q_DECLARE_METATYPE(PhotoInfo)

namespace Scanner
{
// Lower-case suffixes Qt can open here (jpg, png, webp, gif, svg, avif...)
QStringList imageSuffixes();
bool isImageFile(const QString &fileName);

// $XDG_PICTURES_DIR (~/Imagens, ~/Pictures...), falling back to ~/Pictures
QString picturesFolder();

PhotoInfo inspect(const QFileInfo &info);

// Recursive scan, skipping hidden files and folders. known: results of an
// earlier scan (path -> info); unchanged files (same size and mtime) reuse
// it instead of reading EXIF again. batch is called every batchSize photos.
QList<PhotoInfo> scan(const QString &root,
                      const QHash<QString, PhotoInfo> &known = {},
                      const std::function<bool()> &cancelled = {},
                      const std::function<void(const QList<PhotoInfo> &)> &batch = {},
                      int batchSize = 200);

// Images directly inside the folder of path, in natural order (IMG_2 < IMG_10)
QStringList folderImages(const QString &folder);

// Index cache: keeps the EXIF dates between runs
QHash<QString, PhotoInfo> loadIndex(const QString &file);
bool saveIndex(const QString &file, const QList<PhotoInfo> &photos);
}

#endif
