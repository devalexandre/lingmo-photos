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

#ifndef EXIF_H
#define EXIF_H

#include <QByteArray>
#include <QDateTime>
#include <QString>

// Small EXIF reader: Qt only applies the orientation, it doesn't expose the
// date taken or the camera. Reads the TIFF block of JPEG (APP1), TIFF,
// PNG (eXIf) and WebP (EXIF chunk) files through a memory map, so scanning a
// library touches only the first pages of each file.
struct ExifInfo
{
    bool valid = false;
    QDateTime dateTaken;      // DateTimeOriginal, else DateTimeDigitized, else DateTime
    QString make;
    QString model;
    QString lens;
    int orientation = 0;      // 1..8, 0 when absent
    qint64 orientationOffset = -1; // file offset of the 2-byte value (lossless rotation)
    bool bigEndian = false;
    double exposureTime = 0;  // seconds
    double fNumber = 0;
    double focalLength = 0;   // mm
    int iso = 0;

    QString camera() const;
};

namespace Exif
{
ExifInfo read(const QString &path);
// tiff: the TIFF block (starts with II*\0 or MM\0*); base: its offset in the file
ExifInfo parseTiff(const char *data, qint64 size, qint64 base = 0);
// "2024:05:17 10:20:30" -> local QDateTime
QDateTime parseDateTime(const QByteArray &value);

// Orientation after rotating the displayed image by quarterTurns * 90° clockwise
int rotatedOrientation(int orientation, int quarterTurns);
// Writes a new orientation in place; only works when the tag already exists
bool writeOrientation(const QString &path, int orientation);

// JPEG re-encoding (rotate, crop) keeps the photo details: the original APP1
// Exif segment (marker included) with its orientation reset to 1, and a
// helper that puts it right after the SOI marker of the new file.
QByteArray jpegExifSegment(const QString &path);
QByteArray withExifSegment(const QByteArray &jpeg, const QByteArray &segment);
}

#endif
