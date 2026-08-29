/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "hellottsprovider.h"

#include "provideroptions.h"
#include "tts/voice.h"

#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QTimer>

namespace
{
// Kept in step with hellottsplugin.cpp, which declares it.
const QString outputDirectoryOption = QStringLiteral("outputDirectory");

const QString spokenFileName = QStringLiteral("crow-hello-tts.txt");

QList<Voice> exampleVoices()
{
    return {
        Voice(QStringLiteral("Ink"), Language(QLocale::English)),
        Voice(QStringLiteral("Pencil"), Language(QLocale::German)),
    };
}
}

HelloTtsProvider::HelloTtsProvider(QObject *parent)
    : ATTSProvider(parent)
    , m_outputDirectory{QDir::tempPath()}
    , m_language{Language(QLocale::system())}
    , m_voice{exampleVoices().first()}
{
}

QString HelloTtsProvider::getProviderType() const
{
    return QStringLiteral("HelloTtsProvider");
}

// The only method here doing any work, and the shape is what matters rather
// than the writing: go to Speaking, do the thing, come back to Ready.
void HelloTtsProvider::say(const QString &text)
{
    QFile file(QDir(m_outputDirectory).filePath(spokenFileName));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        // An engine that cannot start says so through its own state rather
        // than putting a dialog up: a backend has no idea whether there is a
        // window to put one in. UserNotifier is there for anything the user
        // needs told in words.
        setState(QTextToSpeech::Error);
        return;
    }

    QTextStream out(&file);
    out << m_voice.name() << ": " << text << '\n';
    file.close();

    setState(QTextToSpeech::Speaking);

    // Back to Ready on the next turn of the event loop rather than before this
    // returns. A caller that connected to stateChanged() and *then* called
    // say() would never see a transition emitted from inside the call - and
    // the CLI, which waits for Ready before it exits, would wait for good.
    QTimer::singleShot(0, this, [this] {
        if (m_state == QTextToSpeech::Speaking)
            setState(QTextToSpeech::Ready);
    });
}

void HelloTtsProvider::setState(QTextToSpeech::State newState)
{
    if (m_state == newState)
        return;
    m_state = newState;
    emit stateChanged(m_state);
}

void HelloTtsProvider::stop()
{
    setState(QTextToSpeech::Ready);
}

void HelloTtsProvider::pause()
{
    if (m_state == QTextToSpeech::Speaking)
        setState(QTextToSpeech::Paused);
}

void HelloTtsProvider::resume()
{
    if (m_state == QTextToSpeech::Paused)
        setState(QTextToSpeech::Speaking);
}

QTextToSpeech::State HelloTtsProvider::state() const
{
    return m_state;
}

QTextToSpeech::ErrorReason HelloTtsProvider::errorReason() const
{
    return m_state == QTextToSpeech::Error ? QTextToSpeech::ErrorReason::Playback : QTextToSpeech::ErrorReason::NoError;
}

QString HelloTtsProvider::errorString() const
{
    if (m_state != QTextToSpeech::Error)
        return {};
    return tr("Could not write to %1.").arg(m_outputDirectory);
}

Language HelloTtsProvider::language() const
{
    return m_language;
}

void HelloTtsProvider::setLanguage(const Language &language)
{
    m_language = language;
}

Voice HelloTtsProvider::voice() const
{
    return m_voice;
}

void HelloTtsProvider::setVoice(const Voice &voice)
{
    m_voice = voice;
}

QList<Voice> HelloTtsProvider::availableVoices() const
{
    return exampleVoices();
}

// A caller asks for the voices that fit a language and takes the first. An
// empty answer is allowed and means "use whatever is set"; it is not an error.
QList<Voice> HelloTtsProvider::findVoices(const Language &language) const
{
    QList<Voice> matching;
    const QList<Voice> voices = exampleVoices();
    for (const Voice &candidate : voices) {
        if (candidate.language().toCode() == language.toCode())
            matching.append(candidate);
    }
    return matching;
}

double HelloTtsProvider::rate() const
{
    return m_rate;
}

void HelloTtsProvider::setRate(double rate)
{
    m_rate = rate;
}

double HelloTtsProvider::pitch() const
{
    return m_pitch;
}

void HelloTtsProvider::setPitch(double pitch)
{
    m_pitch = pitch;
}

double HelloTtsProvider::volume() const
{
    return m_volume;
}

void HelloTtsProvider::setVolume(double volume)
{
    m_volume = volume;
}

// Emptiness here is not "no opinion": a caller reads it as an engine with
// nothing installed to speak with and refuses to start.
QList<Language> HelloTtsProvider::availableLanguages() const
{
    return {Language(QLocale::English), Language(QLocale::German), Language(QLocale::system())};
}

void HelloTtsProvider::applyOptions(const ProviderOptions &options)
{
    if (!options.hasOption(outputDirectoryOption))
        return;

    // Declared as a directory, so the value arrives as the QByteArray the
    // settings dialog's browse button stores. An empty one means the user has
    // not chosen; keep the default rather than writing to the root.
    const QString chosen = QString::fromUtf8(options.getOption(outputDirectoryOption).toByteArray());
    if (!chosen.isEmpty())
        m_outputDirectory = chosen;
}

std::unique_ptr<ProviderOptions> HelloTtsProvider::getDefaultOptions() const
{
    auto options = std::make_unique<ProviderOptions>();
    options->setOption(outputDirectoryOption, QByteArray());
    return options;
}

QStringList HelloTtsProvider::getAvailableOptions() const
{
    return {outputDirectoryOption};
}

ProviderCapabilities HelloTtsProvider::capabilities() const
{
    // Matches the "capabilities" array in hello.json. The metadata is what a
    // settings dialog reads before anything is loaded; this is what it reads
    // afterwards, and the two disagreeing is a bug nobody sees until a control
    // is missing.
    return ProviderCapability::VoiceSelection;
}

// Speakers are voices within a voice - a multi-speaker neural model has them,
// most engines do not. Nothing to answer is the normal answer.
QStringList HelloTtsProvider::availableSpeakers() const
{
    return {};
}

QStringList HelloTtsProvider::availableSpeakersForVoice(const Voice &voice) const
{
    Q_UNUSED(voice)
    return availableSpeakers();
}

QString HelloTtsProvider::currentSpeaker() const
{
    return {};
}

void HelloTtsProvider::setSpeaker(const QString &speakerName)
{
    Q_UNUSED(speakerName)
}
