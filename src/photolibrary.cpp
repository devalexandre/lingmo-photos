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

#include "photolibrary.h"

#include <QCollator>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QPointer>
#include <QSettings>
#include <QStandardPaths>
#include <QtConcurrent>

#include <algorithm>

// ---------------------------------------------------------------- RowsModel

int RowsModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

QVariant RowsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size())
        return {};
    const DateGroups::Row &row = m_rows.at(index.row());
    switch (role) {
    case HeaderRole: return row.header;
    case TitleRole: return row.title;
    case GroupCountRole: return row.groupCount;
    case FirstRole: return row.first;
    case CountRole: return row.count;
    default: return {};
    }
}

QHash<int, QByteArray> RowsModel::roleNames() const
{
    return {{HeaderRole, "header"}, {TitleRole, "title"}, {GroupCountRole, "groupCount"},
            {FirstRole, "first"}, {CountRole, "count"}};
}

void RowsModel::setRows(const QList<DateGroups::Row> &rows)
{
    // Same shape (e.g. a favourite toggled): only refresh the data
    if (rows.size() == m_rows.size() && !rows.isEmpty()) {
        m_rows = rows;
        emit dataChanged(index(0), index(rows.size() - 1));
        return;
    }
    beginResetModel();
    m_rows = rows;
    endResetModel();
}

// ------------------------------------------------------------- FoldersModel

int FoldersModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_folders.size();
}

QVariant FoldersModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_folders.size())
        return {};
    const Folder &f = m_folders.at(index.row());
    switch (role) {
    case PathRole: return f.path;
    case NameRole: return f.name;
    case CountRole: return f.count;
    case CoverRole: return f.cover;
    case RelativeRole: return f.relative;
    default: return {};
    }
}

QHash<int, QByteArray> FoldersModel::roleNames() const
{
    return {{PathRole, "path"}, {NameRole, "name"}, {CountRole, "count"},
            {CoverRole, "cover"}, {RelativeRole, "relative"}};
}

void FoldersModel::setFolders(const QList<Folder> &folders)
{
    beginResetModel();
    m_folders = folders;
    endResetModel();
}

// ------------------------------------------------------------- PhotoLibrary

PhotoLibrary::PhotoLibrary(const QString &root, QObject *parent)
    : QObject(parent)
    , m_root(QDir::cleanPath(root.isEmpty() ? Scanner::picturesFolder() : root))
{
    const QStringList favorites = loadFavorites();
    m_favorites = QSet<QString>(favorites.begin(), favorites.end());
    m_rebuildTimer.setSingleShot(true);
    m_rebuildTimer.setInterval(150);
    connect(&m_rebuildTimer, &QTimer::timeout, this, &PhotoLibrary::rebuild);
}

PhotoLibrary::~PhotoLibrary()
{
    if (m_cancel)
        m_cancel->store(true);
    m_future.waitForFinished();
}

QString PhotoLibrary::indexFile()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation)
        + QStringLiteral("/lingmo-photos/index.dat");
}

QStringList PhotoLibrary::loadFavorites()
{
    QSettings settings(QStringLiteral("lingmoos"), QStringLiteral("lingmo-photos"));
    return settings.value(QStringLiteral("Favorites")).toStringList();
}

void PhotoLibrary::saveFavorites()
{
    QSettings settings(QStringLiteral("lingmoos"), QStringLiteral("lingmo-photos"));
    QStringList list(m_favorites.begin(), m_favorites.end());
    list.sort();
    settings.setValue(QStringLiteral("Favorites"), list);
}

void PhotoLibrary::start()
{
    if (!m_loaded && !m_scanning)
        rescan();
}

void PhotoLibrary::rescan()
{
    if (m_cancel)
        m_cancel->store(true);
    const int generation = ++m_generation;
    auto cancel = std::make_shared<std::atomic_bool>(false);
    m_cancel = cancel;
    const bool firstScan = !m_loaded;
    m_scanning = true;
    emit scanningChanged();

    QPointer<PhotoLibrary> self(this);
    const QString root = m_root;
    m_future = QtConcurrent::run([self, root, cancel, generation, firstScan] {
        const QHash<QString, PhotoInfo> known = Scanner::loadIndex(indexFile());
        auto batch = [&](const QList<PhotoInfo> &photos) {
            // The first scan fills the grid while it runs; later ones swap at the end
            if (!firstScan)
                return;
            QMetaObject::invokeMethod(qApp, [self, generation, photos] {
                if (self)
                    self->applyBatch(generation, photos);
            }, Qt::QueuedConnection);
        };
        const QList<PhotoInfo> all = Scanner::scan(root, known, [&] { return cancel->load(); }, batch);
        if (cancel->load())
            return;
        Scanner::saveIndex(indexFile(), all);
        QMetaObject::invokeMethod(qApp, [self, generation, all] {
            if (self)
                self->finishScan(generation, all);
        }, Qt::QueuedConnection);
    });
}

void PhotoLibrary::applyBatch(int generation, const QList<PhotoInfo> &batch)
{
    if (generation != m_generation)
        return;
    m_all.append(batch);
    if (!m_rebuildTimer.isActive())
        m_rebuildTimer.start();
}

void PhotoLibrary::finishScan(int generation, const QList<PhotoInfo> &all)
{
    if (generation != m_generation)
        return;
    m_rebuildTimer.stop();
    m_all = all;
    m_scanning = false;
    m_loaded = true;
    rebuild();
    emit scanningChanged();
}

bool PhotoLibrary::matches(const PhotoInfo &photo) const
{
    if (m_favoritesOnly && !m_favorites.contains(photo.path))
        return false;
    if (!m_folder.isEmpty() && photo.folder != m_folder)
        return false;
    if (m_search.isEmpty())
        return true;
    const QStringList words = m_search.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    const QDate date = photo.date().date();
    const QString haystack = photo.fileName() + QLatin1Char(' ')
        + QDir(m_root).relativeFilePath(photo.folder) + QLatin1Char(' ')
        + DateGroups::monthLabel(DateGroups::monthKey(photo.date())) + QLatin1Char(' ')
        + date.toString(Qt::ISODate) + QLatin1Char(' ')
        + QLocale().toString(date, QLocale::ShortFormat);
    for (const QString &word : words) {
        if (!haystack.contains(word, Qt::CaseInsensitive))
            return false;
    }
    return true;
}

void PhotoLibrary::rebuild()
{
    DateGroups::sortNewestFirst(m_all);
    m_visible.clear();
    for (const PhotoInfo &photo : std::as_const(m_all)) {
        if (matches(photo))
            m_visible.append(photo);
    }
    m_groups = DateGroups::group(m_visible);
    rebuildRows();

    // Folders page: every folder with photos, the newest one as cover
    QHash<QString, FoldersModel::Folder> byPath;
    for (const PhotoInfo &photo : std::as_const(m_all)) {
        FoldersModel::Folder &f = byPath[photo.folder];
        if (f.count++ == 0) {
            f.path = photo.folder;
            f.cover = photo.path; // m_all is newest first
            f.name = photo.folder == m_root ? QFileInfo(m_root).fileName() : QFileInfo(photo.folder).fileName();
            f.relative = QDir(m_root).relativeFilePath(photo.folder);
        }
    }
    QList<FoldersModel::Folder> folders = byPath.values();
    QCollator collator(QLocale().language() == QLocale::C ? QLocale(QLocale::English) : QLocale());
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(folders.begin(), folders.end(), [&](const FoldersModel::Folder &a, const FoldersModel::Folder &b) {
        if ((a.path == m_root) != (b.path == m_root))
            return a.path == m_root;
        return collator.compare(a.relative, b.relative) < 0;
    });
    m_folders.setFolders(folders);

    emit photosChanged();
    emit viewChanged();
    bump(); // rows may keep their shape while showing other photos
}

void PhotoLibrary::rebuildRows()
{
    m_rows.setRows(DateGroups::rows(m_groups, m_columns));
}

void PhotoLibrary::bump()
{
    ++m_revision;
    emit revisionChanged();
}

void PhotoLibrary::setSearchText(const QString &text)
{
    if (m_search == text.trimmed())
        return;
    m_search = text.trimmed();
    emit filterChanged();
    rebuild();
}

void PhotoLibrary::setFavoritesOnly(bool on)
{
    if (m_favoritesOnly == on)
        return;
    m_favoritesOnly = on;
    emit filterChanged();
    rebuild();
}

void PhotoLibrary::setFolderFilter(const QString &folder)
{
    if (m_folder == folder)
        return;
    m_folder = folder;
    emit filterChanged();
    rebuild();
}

void PhotoLibrary::setColumns(int columns)
{
    columns = std::max(1, columns);
    if (m_columns == columns)
        return;
    m_columns = columns;
    emit columnsChanged();
    rebuildRows();
}

QString PhotoLibrary::path(int index) const
{
    return index >= 0 && index < m_visible.size() ? m_visible.at(index).path : QString();
}

QString PhotoLibrary::fileName(int index) const
{
    return index >= 0 && index < m_visible.size() ? m_visible.at(index).fileName() : QString();
}

QStringList PhotoLibrary::visiblePaths() const
{
    QStringList paths;
    paths.reserve(m_visible.size());
    for (const PhotoInfo &p : m_visible)
        paths << p.path;
    return paths;
}

int PhotoLibrary::indexOfPath(const QString &path) const
{
    for (int i = 0; i < m_visible.size(); ++i) {
        if (m_visible.at(i).path == path)
            return i;
    }
    return -1;
}

int PhotoLibrary::rowForPhoto(int index) const
{
    if (index < 0 || index >= m_visible.size())
        return -1;
    const QList<DateGroups::Row> rows = DateGroups::rows(m_groups, m_columns);
    for (int r = 0; r < rows.size(); ++r) {
        const DateGroups::Row &row = rows.at(r);
        if (!row.header && index >= row.first && index < row.first + row.count)
            return r;
    }
    return -1;
}

void PhotoLibrary::setFavorite(const QString &path, bool favorite)
{
    setFavorites({path}, favorite);
}

void PhotoLibrary::setFavorites(const QStringList &paths, bool favorite)
{
    bool changed = false;
    for (const QString &path : paths) {
        if (path.isEmpty())
            continue;
        if (favorite && !m_favorites.contains(path)) {
            m_favorites.insert(path);
            changed = true;
        } else if (!favorite && m_favorites.remove(path)) {
            changed = true;
        }
    }
    if (!changed)
        return;
    saveFavorites();
    emit favoritesChanged();
    if (m_favoritesOnly)
        rebuild();
    bump();
}

bool PhotoLibrary::isSelected(int index) const
{
    return index >= 0 && index < m_visible.size() && m_selected.contains(m_visible.at(index).path);
}

void PhotoLibrary::setSelected(int index, bool selected)
{
    if (index < 0 || index >= m_visible.size())
        return;
    const QString &p = m_visible.at(index).path;
    if (selected == m_selected.contains(p))
        return;
    if (selected)
        m_selected.insert(p);
    else
        m_selected.remove(p);
    emit selectionChanged();
    bump();
}

void PhotoLibrary::toggleSelected(int index)
{
    setSelected(index, !isSelected(index));
}

void PhotoLibrary::selectRange(int from, int to)
{
    if (from > to)
        std::swap(from, to);
    from = std::max(0, from);
    to = std::min<int>(to, m_visible.size() - 1);
    for (int i = from; i <= to; ++i)
        m_selected.insert(m_visible.at(i).path);
    emit selectionChanged();
    bump();
}

void PhotoLibrary::selectAll()
{
    selectRange(0, m_visible.size() - 1);
}

void PhotoLibrary::clearSelection()
{
    if (m_selected.isEmpty())
        return;
    m_selected.clear();
    emit selectionChanged();
    bump();
}

QStringList PhotoLibrary::selectedPaths() const
{
    QStringList paths;
    // In grid order, so a slideshow of the selection follows the grid
    for (const PhotoInfo &p : m_visible) {
        if (m_selected.contains(p.path))
            paths << p.path;
    }
    for (const QString &p : m_selected) {
        if (!paths.contains(p))
            paths << p;
    }
    return paths;
}

bool PhotoLibrary::allSelectedFavorite() const
{
    if (m_selected.isEmpty())
        return false;
    for (const QString &p : m_selected) {
        if (!m_favorites.contains(p))
            return false;
    }
    return true;
}

void PhotoLibrary::removePaths(const QStringList &paths)
{
    const QSet<QString> gone(paths.begin(), paths.end());
    const auto before = m_all.size();
    m_all.removeIf([&gone](const PhotoInfo &p) { return gone.contains(p.path); });
    bool selectionTouched = false;
    for (const QString &p : paths)
        selectionTouched |= m_selected.remove(p);
    if (selectionTouched)
        emit selectionChanged();
    if (m_all.size() != before)
        rebuild();
}

void PhotoLibrary::addPaths(const QStringList &paths)
{
    bool changed = false;
    const QString rootPrefix = m_root + QLatin1Char('/');
    for (const QString &path : paths) {
        const QFileInfo info(path);
        if (!info.isFile() || !info.absoluteFilePath().startsWith(rootPrefix)
            || !Scanner::isImageFile(info.fileName()))
            continue;
        m_all.removeIf([&](const PhotoInfo &p) { return p.path == info.absoluteFilePath(); });
        m_all.append(Scanner::inspect(info));
        changed = true;
    }
    if (changed)
        rebuild();
}
