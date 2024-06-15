/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "qgittag.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>

QGitTag::QGitTag(QObject *parent, const QString &privateKey)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
    , m_privateKey(privateKey)
{
}

void QGitTag::get(const QString &repoId, int number)
{
    m_tagNumber = number;

    // Generate URL
    QUrl apiUrl("https://gitlab.com/api/v4/projects/" + repoId + "/releases");

    // Send request
    QNetworkRequest request(apiUrl);

    // Set private key header
    if (!m_privateKey.isEmpty())
        request.setRawHeader("PRIVATE-TOKEN", m_privateKey.toUtf8());

    // Make request
    m_network->get(request);
    QObject::connect(m_network, &QNetworkAccessManager::finished, this, &QGitTag::parseReply);
}

const QString &QGitTag::name() const
{
    return m_name;
}

const QString &QGitTag::tagName() const
{
    return m_tagName;
}

const QString &QGitTag::body() const
{
    return m_body;
}

QUrl QGitTag::url() const
{
    return m_url;
}

QUrl QGitTag::tarUrl() const
{
    return m_tarUrl;
}

QUrl QGitTag::zipUrl() const
{
    return m_zipUrl;
}

const QList<QGitAsset> &QGitTag::assets() const
{
    return m_assets;
}

int QGitTag::assetId(const QString &str) const
{
    for (int i = 0; i < m_assets.size(); ++i) {
        if (m_assets.at(i).name().contains(str))
            return i;
    }

    return -1; // No asset found
}

QDateTime QGitTag::createdAt() const
{
    return m_createdAt;
}

QDateTime QGitTag::publishedAt() const
{
    return m_publishedAt;
}

int QGitTag::id() const
{
    return m_id;
}

int QGitTag::tagNumber() const
{
    return m_tagNumber;
}

bool QGitTag::draft() const
{
    return m_draft;
}

bool QGitTag::prerelease() const
{
    return m_prerelease;
}

QGitTag::RequestError QGitTag::error() const
{
    return m_error;
}

const QString &QGitTag::errorString() const
{
    return m_errorString;
}

void QGitTag::parseReply(QNetworkReply *reply)
{
    reply->deleteLater();

    // Check for network error
    if (reply->error() != QNetworkReply::NoError) {
        setError(NetworkError, reply->errorString());
        return;
    }

    // Convert to JsonArray
    QJsonDocument jsonResponse = QJsonDocument::fromJson(reply->readAll());
    QJsonArray jsonData = jsonResponse.array();

    // Check if release number exists
    if (jsonData.at(m_tagNumber).type() == QJsonValue::Undefined) {
        setError(NoRelease, tr("Release number %1 is missing").arg(m_tagNumber));
        return;
    }

    QJsonObject release = jsonData.at(m_tagNumber).toObject();
    m_name = release[QStringLiteral("name")].toString();
    m_tagName = release[QStringLiteral("tag_name")].toString();
    m_body = release[QStringLiteral("body")].toString();

    m_url = release[QStringLiteral("html_url")].toString();
    m_tarUrl = release[QStringLiteral("tarball_url")].toString();
    m_zipUrl = release[QStringLiteral("zipball_url")].toString();

    m_createdAt = QDateTime::fromString(release[QStringLiteral("created_at")].toString(), Qt::ISODate);
    m_publishedAt = QDateTime::fromString(release[QStringLiteral("published_at")].toString(), Qt::ISODate);

    m_id = release[QStringLiteral("id")].toInt();
    m_draft = release[QStringLiteral("draft")].toBool();
    m_prerelease = release[QStringLiteral("prerelease")].toBool();

    for (const QJsonValueRef asset : release[QStringLiteral("assets")].toArray())
        m_assets << QGitAsset(asset.toObject());

    m_error = NoError;
    emit finished();
}

void QGitTag::setError(RequestError errorType, const QString &errorString)
{
    m_error = errorType;
    m_errorString = errorString;

    m_name.clear();
    m_tagName.clear();
    m_body.clear();
    m_url.clear();
    m_tarUrl.clear();
    m_zipUrl.clear();
    m_assets.clear();

    m_createdAt = QDateTime();
    m_publishedAt = QDateTime();

    m_id = 0;
    m_tagNumber = 0;
    m_draft = false;
    m_prerelease = false;

    emit finished();
}
