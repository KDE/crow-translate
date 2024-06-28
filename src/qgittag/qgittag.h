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

/**
 * @brief Provides information about releases from GitHub
 */
class QGitTag : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Indicates all possible error conditions found during the requesting release
     */
    enum RequestError {
        /** No error condition */
        NoError,
        /** Network error */
        NetworkError,
        /** There is no release with the specified number */
        NoRelease
    };

    /**
     * @brief Create object
     *
     * Constructs an object with empty data and with parent.
     *
     * @param parent parent object
     */
    explicit QGitTag(QObject *parent = nullptr);

    /**
     * @brief Get release tag
     *
     * Query information about the release.
     *
     * @param owner profile or organization name
     * @param repo repository name
     * @param number release number (0 is the newest relase)
     */
    void get(const QString &owner, const QString &repo, int number = 0);

    /**
     * @brief Release name
     *
     * @return release name.
     */
    const QString &name() const;

    /**
     * @brief Tag name
     *
     * @return release tag name that usually contains version
     */
    const QString &tagName() const;

    /**
     * @brief Release body
     *
     * @return release body that usually contains changelog
     */
    const QString &body() const;

    /**
     * @brief Release URL
     *
     * @return a link to the page of this release, which can be opened in the browser
     */
    QUrl url() const;

    /**
     * @brief Tar URL
     *
     * @return a link to the source tarball.
     */
    QUrl tarUrl() const;

    /**
     * @brief ZIP URL
     *
     * @return Returns a link to the source zipball
     */
    QUrl zipUrl() const;

    /**
     * @brief Release assets
     *
     * @return list of release assets
     */
    const QList<QGitAsset> &assets() const;

    /**
     * @brief Asset ID
     *
     * Returns the first asset number that constains the specified string.
     *
     * @param str name to search
     * @return asset ID (-1 if not found)
     */
    int assetId(const QString &str) const;

    /**
     * @brief Creation date
     *
     * @return creation time of the last commit on which the release is based.
     */
    QDateTime createdAt() const;

    /**
     * @brief Published date
     *
     * @return release publication time.
     */
    QDateTime publishedAt() const;

    /**
     * @brief Release ID
     *
     * @return release identifier.
     */
    int id() const;

    /**
     * @brief Tag number
     *
     * @return tag number
     */
    int tagNumber() const;

    /**
     * @brief Check if draft
     *
     * @return `true` if the release is a draft, otherwise returns `false`.
     */
    bool isDraft() const;

    /**
     * @brief Check if prerelease
     *
     * @return `true` if the release is a prerelease, otherwise returns `false`.
     */
    bool isPrerelease() const;

    /**
     * @brief Last error
     *
     * error that was found during the processing of last release request.
     * If no error was found, returns QGitTag::NoError.
     * The text of the error can be obtained by errorString().
     *
     * @return last error
     */
    RequestError error() const;

    /**
     * @brief Last error string
     *
     * A human-readable description of the last request error that occurred.
     *
     * @return last error string
     */
    const QString &errorString() const;

signals:
    /**
     * @brief Translation finished
     *
     * This signal is emitted when the reply has finished processing and parsed tag data.
     */
    void finished();

private slots:
    void parseReply(QNetworkReply *reply);

private:
    void setError(RequestError errorType, const QString &errorString);

    QNetworkAccessManager *m_network;

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
