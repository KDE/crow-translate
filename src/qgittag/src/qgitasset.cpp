/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "qgitasset.h"

#include <QJsonObject>

QGitAsset::QGitAsset(const QJsonObject &asset)
    : m_contentType(asset[QStringLiteral("name")].toString())
    , m_type(asset[QStringLiteral("content_type")].toString())
    , m_url(asset[QStringLiteral("browser_download_url")].toString())
    , m_createdAt(QDateTime::fromString(asset[QStringLiteral("created_at")].toString(), Qt::ISODate))
    , m_publishedAt(QDateTime::fromString(asset[QStringLiteral("published_at")].toString(), Qt::ISODate))
    , m_id(asset[QStringLiteral("id")].toInt())
    , m_size(asset[QStringLiteral("size")].toInt())
    , m_downloadCount(asset[QStringLiteral("download_count")].toInt())
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
