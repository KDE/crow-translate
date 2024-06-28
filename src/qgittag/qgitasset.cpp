/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "qgitasset.h"

#include <QJsonObject>

QGitAsset::QGitAsset(const QJsonObject &asset)
    : m_contentType(asset[QLatin1String("name")].toString())
    , m_type(asset[QLatin1String("content_type")].toString())
    , m_url(asset[QLatin1String("browser_download_url")].toString())
    , m_createdAt(QDateTime::fromString(asset[QLatin1String("created_at")].toString(), Qt::ISODate))
    , m_publishedAt(QDateTime::fromString(asset[QLatin1String("published_at")].toString(), Qt::ISODate))
    , m_id(asset[QLatin1String("id")].toInt())
    , m_size(asset[QLatin1String("size")].toInt())
    , m_downloadCount(asset[QLatin1String("download_count")].toInt())
{
}

const QString &QGitAsset::name() const
{
    return m_contentType;
}

const QString &QGitAsset::contentType() const
{
    return m_type;
}

QUrl QGitAsset::url() const
{
    return m_url;
}

QDateTime QGitAsset::createdAt() const
{
    return m_createdAt;
}

QDateTime QGitAsset::publishedAt() const
{
    return m_publishedAt;
}

int QGitAsset::id() const
{
    return m_id;
}

int QGitAsset::size() const
{
    return m_size;
}

int QGitAsset::downloadCount() const
{
    return m_downloadCount;
}
