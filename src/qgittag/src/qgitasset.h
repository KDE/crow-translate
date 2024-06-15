/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef QGITASSET_H
#define QGITASSET_H

#include <QDateTime>
#include <QUrl>

class QJsonObject;

class QGitAsset
{
public:
    QGitAsset(const QJsonObject &asset);

    QString name() const;
    QString contentType() const;
    QUrl url() const;

    QDateTime createdAt() const;
    QDateTime publishedAt() const;

    int id() const;
    int size() const;
    int downloadCount() const;

private:
    QString m_contentType;
    QString m_type;
    QUrl m_url;

    QDateTime m_createdAt;
    QDateTime m_publishedAt;

    int m_id;
    int m_size;
    int m_downloadCount;
};

#endif // QGITASSET_H
