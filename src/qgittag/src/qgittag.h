/*
 *  Copyright © 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 *  Copyright © 2020 Volk Milit <javirrdar@gmail.com>
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

#ifndef QGITTAG_H
#define QGITTAG_H

#include <QObject>

#include "qgitasset.h"

class QNetworkAccessManager;
class QNetworkReply;

class QGitTag : public QObject
{
    Q_OBJECT

public:
    enum RequestError {
        NoError,
        NetworkError,
        NoRelease
    };

    QGitTag(QObject *parent = nullptr, const QString &privateKey = QString());
    void get(const QString &repoId, int number = 0);

    QString name() const;
    QString tagName() const;
    QString body() const;

    QUrl url() const;
    QUrl tarUrl() const;
    QUrl zipUrl() const;

    QList<QGitAsset> assets() const;
    int assetId(const QString &str) const;

    QDateTime createdAt() const;
    QDateTime publishedAt() const;

    int id() const;
    int tagNumber() const;
    bool draft() const;
    bool prerelease() const;

    RequestError error() const;
    QString errorString() const;

signals:
    void finished();

private slots:
    void parseReply(QNetworkReply *reply);

private:
    void setError(RequestError errorType, const QString &errorString);

    QNetworkAccessManager *m_network;
    QString m_privateKey;

    QString m_name;
    QString m_tagName;
    QString m_body;

    QUrl m_url;
    QUrl m_tarUrl;
    QUrl m_zipUrl;
    QList<QGitAsset> m_assets;

    QDateTime m_createdAt;
    QDateTime m_publishedAt;

    int m_id = 0;
    int m_tagNumber = 0;
    bool m_draft = false;
    bool m_prerelease = false;

    RequestError m_error = NoError;
    QString m_errorString;
};

#endif // QGITTAG_H
