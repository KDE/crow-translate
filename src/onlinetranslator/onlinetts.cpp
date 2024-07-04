/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "onlinetranslator/onlinetts.h"

#include <QMetaEnum>
#include <QUrl>

OnlineTts::OnlineTts(QObject *parent)
    : QObject(parent)
{
}

void OnlineTts::generateUrls(const QString &instanceUrl, const QString &text, OnlineTranslator::Engine engine, OnlineTranslator::Language lang)
{
    // Get speech
    QString unparsedText = text;
    switch (engine) {
    case OnlineTranslator::Reverso:
    case OnlineTranslator::Google: {
        const QString langString = OnlineTranslator::languageApiCode(engine, lang);
        const QString engineString = QString(QMetaEnum::fromType<OnlineTranslator::Engine>().valueToKey(engine)).toLower();

        // Limit characters per tts request. If the query is larger, then it should be splited into several
        while (!unparsedText.isEmpty()) {
            const int splitIndex = OnlineTranslator::getSplitIndex(unparsedText, s_TtsLimit); // Split the part by special symbol

            // Generate URL API for add it to the playlist
            QUrl apiUrl(QStringLiteral("%1/api/tts").arg(instanceUrl));
            const QString query = QStringLiteral("engine=%1&lang=%2&text=%3").arg(engineString, langString, QString(QUrl::toPercentEncoding(unparsedText.left(splitIndex))));
            apiUrl.setQuery(query);
            m_media.append(apiUrl);

            // Remove the said part from the next saying
            unparsedText = unparsedText.mid(splitIndex);
        }
        break;
    }
    case OnlineTranslator::Yandex: // For some reason, Yandex doesn't supprt TTS in Mozhi
    case OnlineTranslator::Deepl:
    case OnlineTranslator::LibreTranslate:
    case OnlineTranslator::Duckduckgo:
    case OnlineTranslator::Mymemory:
        setError(UnsupportedEngine, tr("%1 engine does not support TTS").arg(QMetaEnum::fromType<OnlineTranslator::Engine>().valueToKey(engine)));
        break;
    }
}

const QList<QMediaContent> &OnlineTts::media() const
{
    return m_media;
}

const QString &OnlineTts::errorString() const
{
    return m_errorString;
}

OnlineTts::TtsError OnlineTts::error() const
{
    return m_error;
}

void OnlineTts::setError(TtsError error, const QString &errorString)
{
    m_error = error;
    m_errorString = errorString;
}
