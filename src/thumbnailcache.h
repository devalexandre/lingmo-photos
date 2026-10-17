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

#ifndef THUMBNAILCACHE_H
#define THUMBNAILCACHE_H

#include <QImage>
#include <QString>

// Thumbnails stored as the freedesktop.org Thumbnail Managing Standard says:
// $XDG_CACHE_HOME/thumbnails/<flavor>/<md5 of the file URI>.png with the
// Thumb::URI and Thumb::MTime text keys, so every spec-following app
// (file managers, image viewers) shares them.
namespace Thumbnails
{
// 128 -> normal, 256 -> large, 512 -> x-large, 1024 -> xx-large
QString flavorFor(int size);
int flavorSize(const QString &flavor);
QString cacheRoot();
QString uriFor(const QString &path);
QString fileNameFor(const QString &path);          // md5(uri) + ".png"
QString pathFor(const QString &path, int size);    // where the thumbnail goes
QString failPathFor(const QString &path);          // fail/lingmo-photos-1.0/...

// True when thumbPath exists and was made from the current version of source
bool isValid(const QString &thumbPath, const QString &source);

// Returns a valid thumbnail (reading the cache or creating it); null on failure.
// size is the wanted bounding box in device pixels; the matching flavor is used.
QImage load(const QString &source, int size);
}

#endif
