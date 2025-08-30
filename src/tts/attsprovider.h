/*
 * SPDX-FileCopyrightText: 2025 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef ATTSPROVIDER_H
#define ATTSPROVIDER_H

#include "language.h"
#include "voice.h"

#include <QLocale>
#include <QObject>
#include <QState>
#include <QStateMachine>
#include <QTextToSpeech>

#include <memory>

class ProviderOptions;
struct ProviderUIRequirements;

class ATTSProvider : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(ATTSProvider)

public:
    enum class ProviderBackend : uint8_t {
        None,
        Mozhi,
        Qt,
        Piper
    };
    Q_ENUM(ProviderBackend)

    static ATTSProvider *createTTSProvider(QObject *parent = nullptr, ProviderBackend chosenBackend = ProviderBackend::None);
    static void resetProblematicProvider(ProviderBackend backend);
    virtual QString getProviderType() const = 0;

    virtual void say(const QString &text) = 0;
    virtual void stop() = 0;
    virtual void pause() = 0;
    virtual void resume() = 0;

    virtual QTextToSpeech::State state() const = 0;
    virtual QTextToSpeech::ErrorReason errorReason() const = 0;
    virtual QString errorString() const = 0;

    virtual Language language() const = 0;
    virtual void setLanguage(const Language &language) = 0;

    virtual Voice voice() const = 0;
    virtual void setVoice(const Voice &voice) = 0;
    virtual QList<Voice> availableVoices() const = 0;
    virtual QList<Voice> findVoices(const Language &language) const = 0;

    virtual double rate() const = 0;
    virtual void setRate(double rate) = 0;

    virtual double pitch() const = 0;
    virtual void setPitch(double pitch) = 0;

    virtual double volume() const = 0;
    virtual void setVolume(double volume) = 0;

    virtual QList<Language> availableLanguages() const = 0;

    // Options system
    virtual void applyOptions(const ProviderOptions &options) = 0;
    virtual std::unique_ptr<ProviderOptions> getDefaultOptions() const = 0;
    virtual QStringList getAvailableOptions() const = 0;

    // UI requirements
    virtual ProviderUIRequirements getUIRequirements() const = 0;

    // Speaker support
    virtual QStringList availableSpeakers() const = 0;
    virtual QStringList availableSpeakersForVoice(const Voice &voice) const = 0;
    virtual QString currentSpeaker() const = 0;
    virtual void setSpeaker(const QString &speakerName) = 0;

    void speak(const QString &toSpeak)
    {
        say(toSpeak);
    }

protected:
    explicit ATTSProvider(QObject *parent = nullptr);

private:
signals:
    void stateChanged(QTextToSpeech::State state);
    void errorOccurred(QTextToSpeech::ErrorReason reason, const QString &errorString);
    void sayingWord(const QString &word, qsizetype start, qsizetype length);
};

#endif // ATTSPROVIDER_H
