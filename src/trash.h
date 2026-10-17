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

#ifndef TRASH_H
#define TRASH_H

#include <QHash>
#include <QObject>
#include <QStringList>

// Moves files to the freedesktop.org trash (the one the file manager shows)
// and remembers each batch so the "Undo" of the toast can put them back.
class Trash : public QObject
{
    Q_OBJECT

public:
    explicit Trash(QObject *parent = nullptr);

    // Returns a batch id (> 0) for undo, or 0 when nothing was moved
    Q_INVOKABLE int moveToTrash(const QStringList &paths);
    // Puts the batch back; returns the restored paths
    Q_INVOKABLE QStringList undo(int batch);
    Q_INVOKABLE bool canUndo(int batch) const { return m_batches.contains(batch); }
    // Paths of the last moveToTrash call that couldn't be moved
    Q_INVOKABLE QStringList lastFailed() const { return m_failed; }

    // Where a trashed file's .trashinfo lives (…/Trash/info/<name>.trashinfo)
    static QString infoPathFor(const QString &pathInTrash);

signals:
    void trashed(const QStringList &paths);
    void restored(const QStringList &paths);

private:
    struct Item
    {
        QString original;
        QString inTrash;
    };
    QHash<int, QList<Item>> m_batches;
    QStringList m_failed;
    int m_nextBatch = 1;
};

#endif
