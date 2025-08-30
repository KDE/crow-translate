/*
 * SPDX-FileCopyrightText: 2025 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "noopttsprovider.h"

#include "provideroptions.h"
#include "provideroptionsmanager.h"
#include "tts/voice.h"

NoopTTSProvider::NoopTTSProvider(QObject *parent)
    : ATTSProvider(parent)
{
}

QString NoopTTSProvider::getProviderType() const
{
    return QStringLiteral("NoopTTSProvider");
}

void NoopTTSProvider::say(const QString &text)
{
    Q_UNUSED(text)
}

void NoopTTSProvider::stop()
{
}

void NoopTTSProvider::pause()
{
}

void NoopTTSProvider::resume()
{
}

QTextToSpeech::State NoopTTSProvider::state() const
{
    return QTextToSpeech::Ready;
}

QTextToSpeech::ErrorReason NoopTTSProvider::errorReason() const
{
    return QTextToSpeech::ErrorReason::NoError;
}

QString NoopTTSProvider::errorString() const
{
    return {};
}

Language NoopTTSProvider::language() const
{
    return Language::autoLanguage();
}

void NoopTTSProvider::setLanguage(const Language &language)
{
    Q_UNUSED(language)
}

Voice NoopTTSProvider::voice() const
{
    return {};
}

void NoopTTSProvider::setVoice(const Voice &voice)
{
    Q_UNUSED(voice)
}

QList<Voice> NoopTTSProvider::availableVoices() const
{
    return {};
}

QList<Voice> NoopTTSProvider::findVoices(const Language &language) const
{
    Q_UNUSED(language)
    return {};
}

double NoopTTSProvider::rate() const
{
    return 0.0;
}

void NoopTTSProvider::setRate(double rate)
{
    Q_UNUSED(rate)
}

double NoopTTSProvider::pitch() const
{
    return 0.0;
}

void NoopTTSProvider::setPitch(double pitch)
{
    Q_UNUSED(pitch)
}

double NoopTTSProvider::volume() const
{
    return 1.0;
}

void NoopTTSProvider::setVolume(double volume)
{
    Q_UNUSED(volume)
}

QList<Language> NoopTTSProvider::availableLanguages() const
{
    return {};
}

void NoopTTSProvider::applyOptions(const ProviderOptions &options)
{
    Q_UNUSED(options)
}

std::unique_ptr<ProviderOptions> NoopTTSProvider::getDefaultOptions() const
{
    return std::make_unique<ProviderOptions>();
}

QStringList NoopTTSProvider::getAvailableOptions() const
{
    return {};
}

ProviderUIRequirements NoopTTSProvider::getUIRequirements() const
{
    return {};
}

QStringList NoopTTSProvider::availableSpeakers() const
{
    return {};
}

QStringList NoopTTSProvider::availableSpeakersForVoice(const Voice &voice) const
{
    Q_UNUSED(voice);
    return availableSpeakers();
}

QString NoopTTSProvider::currentSpeaker() const
{
    return {};
}

void NoopTTSProvider::setSpeaker(const QString &speakerName)
{
    Q_UNUSED(speakerName)
}
