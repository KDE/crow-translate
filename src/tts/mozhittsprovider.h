/*
 * SPDX-FileCopyrightText: 2025 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef MOZHITTSPROVIDER_H
#define MOZHITTSPROVIDER_H

#include "language.h"
#include "onlinetranslator.h"
#include "playlistplayer.h"
#include "tts/attsprovider.h"

#include <QObject>
#include <QTimer>

class MozhiTTSProvider : public ATTSProvider
{
    Q_OBJECT
    Q_DISABLE_COPY(MozhiTTSProvider)

public:
    explicit MozhiTTSProvider(QObject *parent = nullptr);
    ~MozhiTTSProvider() override;

    QString getProviderType() const override;
    void say(const QString &text) override;
    void stop() override;
    void pause() override;
    void resume() override;

    QTextToSpeech::State state() const override;
    QTextToSpeech::ErrorReason errorReason() const override;
    QString errorString() const override;

    Language language() const override;
    void setLanguage(const Language &language) override;

    Voice voice() const override;
    void setVoice(const Voice &voice) override;
    QList<Voice> availableVoices() const override;
    QList<Voice> findVoices(const Language &language) const override;

    double rate() const override;
    void setRate(double rate) override;

    double pitch() const override;
    void setPitch(double pitch) override;

    double volume() const override;
    void setVolume(double volume) override;

    QList<Language> availableLanguages() const override;

    void setInstance(const QString &instance);
    QString instance() const;

    void setEngine(OnlineTranslator::Engine engine);
    OnlineTranslator::Engine engine() const;

    // Options system implementation
    void applyOptions(const ProviderOptions &options) override;
    std::unique_ptr<ProviderOptions> getDefaultOptions() const override;
    QStringList getAvailableOptions() const override;

    // UI requirements
    ProviderUIRequirements getUIRequirements() const override;

    // Speaker support
    QStringList availableSpeakers() const override;
    QStringList availableSpeakersForVoice(const Voice &voice) const override;
    QString currentSpeaker() const override;
    void setSpeaker(const QString &speakerName) override;

private slots:
    void onMediaPlayerStateChanged(QMediaPlayer::PlaybackState state);
    void onMediaPlayerError(QMediaPlayer::Error error, const QString &errorString);
    void onMediaPlayerPositionChanged(qint64 position);
    void onMediaPlayerDurationChanged(qint64 duration);

private:
    QTextToSpeech::State playerStateToTTSState(QMediaPlayer::PlaybackState playerState) const;
    OnlineTranslator::Language localeToOnlineTranslatorLanguage(const QLocale &locale) const;
    QLocale onlineTranslatorLanguageToLocale(OnlineTranslator::Language language) const;
    void updateState(QTextToSpeech::State newState);
    void setError(QTextToSpeech::ErrorReason reason, const QString &message);

    OnlineTranslator *m_translator;
    PlaylistPlayer *m_player;

    QTextToSpeech::State m_currentState;
    QTextToSpeech::ErrorReason m_errorReason;
    QString m_errorString;

    Language m_language;
    OnlineTranslator::Engine m_engine;
    double m_rate;
    double m_pitch;
    double m_volume;

    QString m_currentText;
    qint64 m_totalDuration;
    qint64 m_currentPosition;

signals:
};

#endif // MOZHITTSPROVIDER_H