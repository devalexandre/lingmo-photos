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

#include "thumbnailprovider.h"
#include "thumbnailcache.h"

#include <QRunnable>
#include <QUrl>


namespace {

// Runs on the pool and hands the image to the response through a queued
// signal, so the response may be cancelled/deleted while it decodes.
class ThumbnailJob : public QObject, public QRunnable
{
    Q_OBJECT

public:
    ThumbnailJob(const QString &path, int size)
        : m_path(path), m_size(size)
    {
    }

    void run() override
    {
        emit done(Thumbnails::load(m_path, m_size));
    }

signals:
    void done(const QImage &image);

private:
    QString m_path;
    int m_size;
};

class ThumbnailResponse : public QQuickImageResponse
{
public:
    ThumbnailResponse(const QString &path, int size, QThreadPool *pool)
    {
        auto *job = new ThumbnailJob(path, size);
        connect(job, &ThumbnailJob::done, this, [this](const QImage &image) {
            m_image = image;
            emit finished();
        });
        pool->start(job);
    }

    QQuickTextureFactory *textureFactory() const override
    {
        return QQuickTextureFactory::textureFactoryForImage(m_image);
    }

private:
    QImage m_image;
};

} // namespace

ThumbnailProvider::ThumbnailProvider()
{
    // Decoding is CPU bound; keep a core free for the UI
    m_pool.setMaxThreadCount(std::max(2, QThread::idealThreadCount() - 1));
}

QQuickImageResponse *ThumbnailProvider::requestImageResponse(const QString &id, const QSize &requestedSize)
{
    QString encoded = id;
    const int query = encoded.indexOf(QLatin1Char('?'));
    if (query >= 0)
        encoded.truncate(query);
    const QString path = QUrl::fromPercentEncoding(encoded.toUtf8());
    const int wanted = std::max(requestedSize.width(), requestedSize.height());
    return new ThumbnailResponse(path, wanted > 0 ? wanted : 256, &m_pool);
}

#include "thumbnailprovider.moc"
