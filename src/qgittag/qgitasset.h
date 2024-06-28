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

/**
 * @brief Provides information about tag assets
 *
 * Can be obtained from the QGitTag object, which contains a list of assets for a particular release.
 *
 * Example:
 * @code
 * QGitTag tag;
 * // Obtain information
 *
 * QTextStream out(stdout);
 * out << "Asset: " + tag.assets().at(0).name() << endl; // Prints asset filename
 * out << "Type: " + tag.assets().at(0).type() << endl; // Prints asset type
 * out << "Download Url: " + tag.assets().at(0).url() << endl; // Prints download link
 * @endcode
 */
class QGitAsset
{
public:
    /**
     * @brief Create object
     *
     * Constructs an object from asset. Normally you don't need call this function.
     *
     * @param asset asset object
     */
    explicit QGitAsset(const QJsonObject &asset);

    /**
     * @brief Name
     *
     * @return name of represented asset file
     */
    const QString &name() const;

    /**
     * @brief MIME-type
     *
     * @return asset MIME-type
     */
    const QString &contentType() const;

    /**
     * @brief URL
     *
     * @return asset download link
     */
    QUrl url() const;

    /**
     * @brief Creation date
     *
     * @return asset creation date
     */
    QDateTime createdAt() const;

    /**
     * @brief Publication date
     *
     * @return asset publication date
     */
    QDateTime publishedAt() const;

    /**
     * @brief ID
     *
     * @return asset identifier
     */
    int id() const;

    /**
     * @brief Size
     *
     * @return asset file size
     */
    int size() const;

    /**
     * @brief Downloads count
     *
     * @return number of the the asset downloads
     */
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
