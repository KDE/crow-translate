/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef QGITTAG_H
#define QGITTAG_H

#include "qgitasset.h"

#include <QObject>

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

    explicit QGitTag(QObject *parent = nullptr, const QString &privateKey = QString());
    void get(const QString &repoId, int number = 0);

    const QString &name() const;
    const QString &tagName() const;
    const QString &body() const;

    QUrl url() const;
    QUrl tarUrl() const;
    QUrl zipUrl() const;

    const QList<QGitAsset> &assets() const;
    int assetId(const QString &str) const;

    QDateTime createdAt() const;
    QDateTime publishedAt() const;

    int id() const;
    int tagNumber() const;
    bool draft() const;
    bool prerelease() const;

    RequestError error() const;
    const QString &errorString() const;

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
