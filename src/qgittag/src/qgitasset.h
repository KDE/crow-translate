/*
 *  Copyright © 2018-2019 Hennadii Chernyshchyk <genaloner@gmail.com>
 *
 *  This file is part of Crow Translate.
 *
 *  Crow Translate is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a get of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
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
