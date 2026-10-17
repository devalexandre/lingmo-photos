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

#ifndef PHOTOLIBRARY_H
#define PHOTOLIBRARY_H

#include <QAbstractListModel>
#include <QFuture>
#include <QSet>
#include <QTimer>

#include <atomic>
#include <memory>

#include "dategroups.h"
#include "scanner.h"

// Rows of the library grid (month headers and rows of thumbnails)
class RowsModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles { HeaderRole = Qt::UserRole + 1, TitleRole, GroupCountRole, FirstRole, CountRole };
    using QAbstractListModel::QAbstractListModel;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    void setRows(const QList<DateGroups::Row> &rows);

private:
    QList<DateGroups::Row> m_rows;
};

// Folders that contain photos, for the Folders page
class FoldersModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles { PathRole = Qt::UserRole + 1, NameRole, CountRole, CoverRole, RelativeRole };
    struct Folder
    {
        QString path;
        QString name;
        QString relative;
        QString cover;
        int count = 0;
    };
    using QAbstractListModel::QAbstractListModel;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    void setFolders(const QList<Folder> &folders);

private:
    QList<Folder> m_folders;
};

class PhotoLibrary : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString rootFolder READ rootFolder CONSTANT)
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)
    Q_PROPERTY(bool loaded READ loaded NOTIFY scanningChanged)
    Q_PROPERTY(int totalCount READ totalCount NOTIFY photosChanged)
    Q_PROPERTY(int count READ count NOTIFY viewChanged)
    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY filterChanged)
    Q_PROPERTY(bool favoritesOnly READ favoritesOnly WRITE setFavoritesOnly NOTIFY filterChanged)
    Q_PROPERTY(QString folderFilter READ folderFilter WRITE setFolderFilter NOTIFY filterChanged)
    Q_PROPERTY(int columns READ columns WRITE setColumns NOTIFY columnsChanged)
    Q_PROPERTY(QObject *rows READ rows CONSTANT)
    Q_PROPERTY(QObject *folders READ folders CONSTANT)
    Q_PROPERTY(int favoritesCount READ favoritesCount NOTIFY favoritesChanged)
    Q_PROPERTY(int selectedCount READ selectedCount NOTIFY selectionChanged)
    // Bumped when selection or favourites change: thumbnails re-read their state
    Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)

public:
    explicit PhotoLibrary(const QString &root = QString(), QObject *parent = nullptr);
    ~PhotoLibrary() override;

    QString rootFolder() const { return m_root; }
    bool scanning() const { return m_scanning; }
    bool loaded() const { return m_loaded; }
    int totalCount() const { return m_all.size(); }
    int count() const { return m_visible.size(); }
    QString searchText() const { return m_search; }
    void setSearchText(const QString &text);
    bool favoritesOnly() const { return m_favoritesOnly; }
    void setFavoritesOnly(bool on);
    QString folderFilter() const { return m_folder; }
    void setFolderFilter(const QString &folder);
    int columns() const { return m_columns; }
    void setColumns(int columns);
    QObject *rows() { return &m_rows; }
    QObject *folders() { return &m_folders; }
    int favoritesCount() const { return m_favorites.size(); }
    int selectedCount() const { return m_selected.size(); }
    int revision() const { return m_revision; }

    // Starts the first scan (no-op when already loaded)
    Q_INVOKABLE void start();
    Q_INVOKABLE void rescan();

    // Visible photos (after search/favourites/folder filters), newest first
    Q_INVOKABLE QString path(int index) const;
    Q_INVOKABLE QString fileName(int index) const;
    Q_INVOKABLE QStringList visiblePaths() const;
    Q_INVOKABLE int indexOfPath(const QString &path) const;
    // Grid row that shows the visible photo `index` (-1 when none)
    Q_INVOKABLE int rowForPhoto(int index) const;

    Q_INVOKABLE bool isFavorite(const QString &path) const { return m_favorites.contains(path); }
    Q_INVOKABLE void setFavorite(const QString &path, bool favorite);
    Q_INVOKABLE void setFavorites(const QStringList &paths, bool favorite);

    Q_INVOKABLE bool isSelected(int index) const;
    Q_INVOKABLE void setSelected(int index, bool selected);
    Q_INVOKABLE void toggleSelected(int index);
    Q_INVOKABLE void selectRange(int from, int to);
    Q_INVOKABLE void selectAll();
    Q_INVOKABLE void clearSelection();
    Q_INVOKABLE QStringList selectedPaths() const;
    Q_INVOKABLE bool allSelectedFavorite() const;

    // Keep the model in sync after trashing/restoring/saving copies
    Q_INVOKABLE void removePaths(const QStringList &paths);
    Q_INVOKABLE void addPaths(const QStringList &paths);

    // Settings storage of favourites (tests point it elsewhere)
    static QStringList loadFavorites();

signals:
    void scanningChanged();
    void photosChanged();
    void viewChanged();
    void filterChanged();
    void columnsChanged();
    void favoritesChanged();
    void selectionChanged();
    void revisionChanged();

private:
    void applyBatch(int generation, const QList<PhotoInfo> &batch);
    void finishScan(int generation, const QList<PhotoInfo> &all);
    void rebuild();
    void rebuildRows();
    void bump();
    void saveFavorites();
    bool matches(const PhotoInfo &photo) const;
    static QString indexFile();

    QString m_root;
    QList<PhotoInfo> m_all;
    QList<PhotoInfo> m_visible;
    QList<DateGroups::Group> m_groups;
    QSet<QString> m_favorites;
    QSet<QString> m_selected;
    QString m_search;
    QString m_folder;
    bool m_favoritesOnly = false;
    bool m_scanning = false;
    bool m_loaded = false;
    int m_columns = 5;
    int m_revision = 0;
    int m_generation = 0;
    std::shared_ptr<std::atomic_bool> m_cancel;
    QFuture<void> m_future;
    QTimer m_rebuildTimer;
    RowsModel m_rows;
    FoldersModel m_folders;
};

#endif
