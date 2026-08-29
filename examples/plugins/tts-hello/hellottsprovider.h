/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef HELLOTTSPROVIDER_H
#define HELLOTTSPROVIDER_H

#include "tts/attsprovider.h"

#include <QObject>

#include <memory>

// A speech backend that writes down what it was asked to say.
//
// ATTSProvider is the widest of the three interfaces, because speech has the
// most for a frontend to offer: voices, speakers within a voice, rate, pitch,
// volume, and a state machine the caller waits on. Most of it has an obvious
// nothing-to-do answer, which is what the bulk of this file is - but the
// states are not optional. A caller that asked for speech waits for
// QTextToSpeech::Ready to come back, and a backend that never emits it hangs
// the process that asked.
class HelloTtsProvider : public ATTSProvider
{
    Q_OBJECT

public:
    explicit HelloTtsProvider(QObject *parent = nullptr);

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

    void applyOptions(const ProviderOptions &options) override;
    std::unique_ptr<ProviderOptions> getDefaultOptions() const override;
    QStringList getAvailableOptions() const override;

    ProviderCapabilities capabilities() const override;

    QStringList availableSpeakers() const override;
    QStringList availableSpeakersForVoice(const Voice &voice) const override;
    QString currentSpeaker() const override;
    void setSpeaker(const QString &speakerName) override;

private:
    void setState(QTextToSpeech::State newState);

    QString m_outputDirectory;
    QTextToSpeech::State m_state = QTextToSpeech::Ready;
    Language m_language;
    Voice m_voice;
    double m_rate = 0.0;
    double m_pitch = 0.0;
    double m_volume = 1.0;
};

#endif // HELLOTTSPROVIDER_H
