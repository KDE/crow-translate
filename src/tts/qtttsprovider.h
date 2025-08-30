/*
 * SPDX-FileCopyrightText: 2025 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef QTTTSPROVIDER_H
#define QTTTSPROVIDER_H

#include "tts/attsprovider.h"

#include <QObject>
#include <QTextToSpeech>

class QtTTSProvider : public ATTSProvider
{
    Q_OBJECT
    Q_DISABLE_COPY(QtTTSProvider)

public:
    explicit QtTTSProvider(QObject *parent = nullptr);

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
    void onStateChanged(QTextToSpeech::State state);
    void onErrorOccurred(QTextToSpeech::ErrorReason reason, const QString &errorString);
    void onSayingWord(const QString &word, qsizetype start, qsizetype length);

private:
    QTextToSpeech *m_tts;

#ifdef Q_OS_WIN
    void showWindowsVoiceInstallationDialog(const Language &language) const;
#endif

signals:
};

#endif // QTTTSPROVIDER_H
