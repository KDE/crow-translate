/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef ONLINETTS_H
#define ONLINETTS_H

#include "onlinetranslator.h"

#include <QLocale>
#include <QMediaContent>

/**
 * @brief Provides TTS URL generation
 *
 * Example:
 * @code
 * QMediaPlayer *player = new QMediaPlayer(this);
 * QMediaPlaylist *playlist = new QMediaPlaylist(player);
 * OnlineTts tts;
 *
 * playlist->addMedia(tts.generateUrls("Hello World!", OnlineTranslator::Google));
 * player->setPlaylist(playlist);
 *
 * player->play(); // Plays "Hello World!"
 * @endcode
 */
class OnlineTts : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(OnlineTts)

public:
    /**
     * @brief Indicates all possible error conditions found during the processing of the URLs generation
     */
    enum TtsError {
        /** No error condition */
        NoError,
        /** Specified engine does not support TTS */
        UnsupportedEngine,
        /** Unsupported language by specified engine */
        UnsupportedLanguage,
        /** Unsupported voice by specified engine */
        UnsupportedVoice,
        /** Unsupported emotion by specified engine */
        UnsupportedEmotion
    };

    /**
     * @brief Create object
     *
     * Constructs an object with empty data and with parent.
     * You can use generateUrls() to create URLs for use in QMediaPlayer.
     *
     * @param parent parent object
     */
    explicit OnlineTts(QObject *parent = nullptr);

    /**
     * @brief Create TTS urls
     *
     * Splits text into parts (engines have a limited number of characters per request) and returns list with the generated API URLs to play.
     *
     * @param text text to speak
     * @param engine online translation engine
     * @param lang text language
     * @param voice voice to use (used only by Yandex)
     * @param emotion emotion to use (used only by Yandex)
     */
    void generateUrls(const QString &instanceUrl, const QString &text, OnlineTranslator::Engine engine, OnlineTranslator::Language lang);

    /**
     * @brief Generated media
     *
     * @return List of generated URLs
     */
    const QList<QMediaContent> &media() const;

    /**
     * @brief Last error
     *
     * Error that was found during the generating tts.
     * If no error was found, returns TtsError::NoError.
     * The text of the error can be obtained by errorString().
     *
     * @return last error
     */
    TtsError error() const;

    /**
     * @brief Last error string
     *
     * A human-readable description of the last tts URL generation error that occurred.
     *
     * @return last error string
     */
    const QString &errorString() const;

private:
    void setError(TtsError error, const QString &errorString);

    QString languageApiCode(OnlineTranslator::Engine engine, OnlineTranslator::Language lang);

    static constexpr int s_TtsLimit = 200;

    QList<QMediaContent> m_media;
    QString m_errorString;
    TtsError m_error = NoError;
};

#endif // ONLINETTS_H
