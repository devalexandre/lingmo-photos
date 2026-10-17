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

#include "trash.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

Trash::Trash(QObject *parent)
    : QObject(parent)
{
}

QString Trash::infoPathFor(const QString &pathInTrash)
{
    const QFileInfo file(pathInTrash);
    // <trash>/files/<name>  ->  <trash>/info/<name>.trashinfo
    const QString trashDir = QFileInfo(file.absolutePath()).absolutePath();
    return trashDir + QStringLiteral("/info/") + file.fileName() + QStringLiteral(".trashinfo");
}

int Trash::moveToTrash(const QStringList &paths)
{
    m_failed.clear();
    // Qt can't create the home trash when $XDG_DATA_HOME itself is missing
    QDir().mkpath(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation));
    QList<Item> items;
    QStringList moved;
    for (const QString &path : paths) {
        QString inTrash;
        // Qt follows the freedesktop.org spec: home trash, or .Trash-$UID on other disks
        if (QFile::moveToTrash(path, &inTrash)) {
            items.append({QFileInfo(path).absoluteFilePath(), inTrash});
            moved << path;
        } else {
            m_failed << path;
        }
    }
    if (items.isEmpty())
        return 0;
    const int batch = m_nextBatch++;
    m_batches.insert(batch, items);
    emit trashed(moved);
    return batch;
}

QStringList Trash::undo(int batch)
{
    QStringList restored;
    const QList<Item> items = m_batches.take(batch);
    for (const Item &item : items) {
        // Never overwrite a file that took the old name in the meantime
        if (QFileInfo::exists(item.original) || !QFileInfo::exists(item.inTrash))
            continue;
        QDir().mkpath(QFileInfo(item.original).absolutePath());
        if (QFile::rename(item.inTrash, item.original)) {
            QFile::remove(infoPathFor(item.inTrash));
            restored << item.original;
        }
    }
    if (!restored.isEmpty())
        emit this->restored(restored);
    return restored;
}
