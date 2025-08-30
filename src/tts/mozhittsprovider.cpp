/*
 * SPDX-FileCopyrightText: 2025 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "mozhittsprovider.h"

#include "provideroptions.h"
#include "tts/voice.h"

#include <QAudioOutput>
#include <QDebug>

MozhiTTSProvider::MozhiTTSProvider(QObject *parent)
    : ATTSProvider(parent)
    , m_translator(new OnlineTranslator(this))
    , m_player(new PlaylistPlayer(this))
    , m_currentState(QTextToSpeech::State::Ready)
    , m_errorReason(QTextToSpeech::ErrorReason::NoError)
    , m_language(Language(QLocale::English))
    , m_engine(OnlineTranslator::Engine::Google)
    , m_rate(0.0)
    , m_pitch(0.0)
    , m_volume(1.0)
    , m_totalDuration(0)
    , m_currentPosition(0)
{
    connect(m_player, &QMediaPlayer::playbackStateChanged, this, &MozhiTTSProvider::onMediaPlayerStateChanged);
    connect(m_player, &QMediaPlayer::errorOccurred, this, &MozhiTTSProvider::onMediaPlayerError);
    connect(m_player, &QMediaPlayer::positionChanged, this, &MozhiTTSProvider::onMediaPlayerPositionChanged);
    connect(m_player, &QMediaPlayer::durationChanged, this, &MozhiTTSProvider::onMediaPlayerDurationChanged);

    auto *audioOutput = new QAudioOutput(this);
    audioOutput->setVolume(m_volume);
    m_player->setAudioOutput(audioOutput);
}

MozhiTTSProvider::~MozhiTTSProvider() = default;

QString MozhiTTSProvider::getProviderType() const
{
    return QStringLiteral("MozhiTTSProvider");
}

void MozhiTTSProvider::say(const QString &text)
{
    if (text.isEmpty()) {
        setError(QTextToSpeech::ErrorReason::Input, tr("Text is empty"));
        return;
    }

    m_currentText = text;
    m_player->clearPlaylist();

    const OnlineTranslator::Language otLang = localeToOnlineTranslatorLanguage(m_language.toQLocale());
    qDebug() << "MozhiTTSProvider::say() - Text:" << text.left(50) + (text.length() > 50 ? "..." : "");
    qDebug() << "MozhiTTSProvider::say() - Language:" << m_language.name() << "-> OnlineTranslator:" << static_cast<int>(otLang);
    qDebug() << "MozhiTTSProvider::say() - Engine:" << static_cast<int>(m_engine);
    qDebug() << "MozhiTTSProvider::say() - Instance:" << m_translator->instance();

    const QList<QUrl> urls = m_translator->generateUrls(text, m_engine, otLang);

    qDebug() << "MozhiTTSProvider::say() - OnlineTranslator error after generateUrls:" << m_translator->error() << m_translator->errorString();
    qDebug() << "MozhiTTSProvider::say() - Generated URLs count:" << urls.size();

    for (int i = 0; i < urls.size(); ++i) {
        const QUrl &url = urls[i];
        qDebug() << "MozhiTTSProvider::say() - URL" << i << ":" << url.toString();
        qDebug() << "MozhiTTSProvider::say() - URL" << i << "valid:" << url.isValid() << "scheme:" << url.scheme() << "host:" << url.host();
        if (!url.isValid()) {
            qWarning() << "MozhiTTSProvider::say() - INVALID URL detected at generation time:" << url.toString();
        }
    }

    if (m_translator->error() != OnlineTranslator::NoError) {
        setError(QTextToSpeech::ErrorReason::Configuration, m_translator->errorString());
        return;
    }

    if (urls.isEmpty()) {
        setError(QTextToSpeech::ErrorReason::Input, tr("No audio URLs generated"));
        return;
    }

    m_player->setPlaylist(urls);
    updateState(QTextToSpeech::State::Synthesizing);
    m_player->playPlaylist();
}

void MozhiTTSProvider::stop()
{
    m_player->stop();
    m_player->clearPlaylist();
    updateState(QTextToSpeech::State::Ready);
}

void MozhiTTSProvider::pause()
{
    if (m_currentState == QTextToSpeech::State::Speaking) {
        m_player->pause();
    }
}

void MozhiTTSProvider::resume()
{
    if (m_currentState == QTextToSpeech::State::Paused) {
        m_player->play();
    }
}

QTextToSpeech::State MozhiTTSProvider::state() const
{
    return m_currentState;
}

QTextToSpeech::ErrorReason MozhiTTSProvider::errorReason() const
{
    return m_errorReason;
}

QString MozhiTTSProvider::errorString() const
{
    return m_errorString;
}

Language MozhiTTSProvider::language() const
{
    return m_language;
}

void MozhiTTSProvider::setLanguage(const Language &language)
{
    m_language = language;
}

Voice MozhiTTSProvider::voice() const
{
    return {};
}

void MozhiTTSProvider::setVoice(const Voice &voice)
{
    Q_UNUSED(voice)
}

QList<Voice> MozhiTTSProvider::availableVoices() const
{
    return {};
}

QList<Voice> MozhiTTSProvider::findVoices(const Language &language) const
{
    Q_UNUSED(language)
    return {};
}

double MozhiTTSProvider::rate() const
{
    return m_rate;
}

void MozhiTTSProvider::setRate(double rate)
{
    m_rate = qBound(-1.0, rate, 1.0);
}

double MozhiTTSProvider::pitch() const
{
    return m_pitch;
}

void MozhiTTSProvider::setPitch(double pitch)
{
    m_pitch = qBound(-1.0, pitch, 1.0);
}

double MozhiTTSProvider::volume() const
{
    return m_volume;
}

void MozhiTTSProvider::setVolume(double volume)
{
    m_volume = qBound(0.0, volume, 1.0);
    if (m_player->audioOutput() != nullptr) {
        m_player->audioOutput()->setVolume(static_cast<float>(m_volume));
    }
}

QList<Language> MozhiTTSProvider::availableLanguages() const
{
    QList<Language> languages;

    const QList<OnlineTranslator::Language> supportedLanguages = {OnlineTranslator::Language::English,
                                                                  OnlineTranslator::Language::Spanish,
                                                                  OnlineTranslator::Language::French,
                                                                  OnlineTranslator::Language::German,
                                                                  OnlineTranslator::Language::Italian,
                                                                  OnlineTranslator::Language::PortugueseBrazilian,
                                                                  OnlineTranslator::Language::Russian,
                                                                  OnlineTranslator::Language::Japanese,
                                                                  OnlineTranslator::Language::Korean,
                                                                  OnlineTranslator::Language::ChineseSimplified,
                                                                  OnlineTranslator::Language::ChineseTraditional,
                                                                  OnlineTranslator::Language::Arabic,
                                                                  OnlineTranslator::Language::Hindi,
                                                                  OnlineTranslator::Language::Dutch,
                                                                  OnlineTranslator::Language::Polish,
                                                                  OnlineTranslator::Language::Turkish,
                                                                  OnlineTranslator::Language::Vietnamese,
                                                                  OnlineTranslator::Language::Thai,
                                                                  OnlineTranslator::Language::Czech,
                                                                  OnlineTranslator::Language::Hungarian};

    for (const auto &lang : supportedLanguages) {
        const QLocale locale = onlineTranslatorLanguageToLocale(lang);
        languages.append(Language(locale));
    }

    return languages;
}

void MozhiTTSProvider::setInstance(const QString &instance)
{
    m_translator->setInstance(instance);
}

QString MozhiTTSProvider::instance() const
{
    return m_translator->instance();
}

void MozhiTTSProvider::setEngine(OnlineTranslator::Engine engine)
{
    if (engine == OnlineTranslator::Engine::Google || engine == OnlineTranslator::Engine::Reverso) {
        m_engine = engine;
    } else {
        m_engine = OnlineTranslator::Engine::Google;
    }
}

OnlineTranslator::Engine MozhiTTSProvider::engine() const
{
    return m_engine;
}

void MozhiTTSProvider::onMediaPlayerStateChanged(QMediaPlayer::PlaybackState state)
{
    const QTextToSpeech::State newState = playerStateToTTSState(state);
    updateState(newState);
}

void MozhiTTSProvider::onMediaPlayerError(QMediaPlayer::Error error, const QString &errorString)
{
    QString enhancedErrorString = errorString;

    // Check if this is a network-related error and enhance the message
    if (error == QMediaPlayer::ResourceError) {
        if (errorString.contains("Could not open file")) {
            // For "Could not open file" errors with network URLs, assume it's a server error
            const QUrl currentUrl = m_player->source();
            if (currentUrl.scheme() == "https" || currentUrl.scheme() == "http") {
                enhancedErrorString = tr("Mozhi instance failed: %1 (HTTP Server Error)").arg(m_translator->instance());
            } else {
                enhancedErrorString = tr("Cannot connect to Mozhi instance: %1").arg(m_translator->instance());
            }
        } else if (errorString.contains("Server returned 5XX Server Error")) {
            enhancedErrorString = tr("Mozhi instance failed: %1 (HTTP 500 Internal Server Error)").arg(m_translator->instance());
        } else if (errorString.contains("Server returned 4XX Client Error")) {
            enhancedErrorString = tr("Mozhi instance error: %1 (HTTP 4XX Client Error)").arg(m_translator->instance());
        } else if (errorString.contains("No such file or directory")) {
            enhancedErrorString = tr("Cannot connect to Mozhi instance: %1").arg(m_translator->instance());
        }
    }

    setError(QTextToSpeech::ErrorReason::Playback, enhancedErrorString);
}

void MozhiTTSProvider::onMediaPlayerPositionChanged(qint64 position)
{
    m_currentPosition = position;

    if (m_totalDuration > 0) {
        const QList<QUrl> &playlist = m_player->getPlaylist();

        if (!playlist.isEmpty()) {
            emit sayingWord(m_currentText, 0, m_currentText.length());
        }
    }
}

void MozhiTTSProvider::onMediaPlayerDurationChanged(qint64 duration)
{
    m_totalDuration = duration;
}

QTextToSpeech::State MozhiTTSProvider::playerStateToTTSState(QMediaPlayer::PlaybackState playerState) const
{
    switch (playerState) {
    case QMediaPlayer::PlaybackState::StoppedState:
        return QTextToSpeech::State::Ready;
    case QMediaPlayer::PlaybackState::PlayingState:
        return QTextToSpeech::State::Speaking;
    case QMediaPlayer::PlaybackState::PausedState:
        return QTextToSpeech::State::Paused;
    }
    return QTextToSpeech::State::Ready;
}

OnlineTranslator::Language MozhiTTSProvider::localeToOnlineTranslatorLanguage(const QLocale &locale) const
{
    return OnlineTranslator::language(locale);
}

QLocale MozhiTTSProvider::onlineTranslatorLanguageToLocale(OnlineTranslator::Language language) const
{
    switch (language) {
    case OnlineTranslator::Language::English:
        return QLocale::English;
    case OnlineTranslator::Language::Spanish:
        return QLocale::Spanish;
    case OnlineTranslator::Language::French:
        return QLocale::French;
    case OnlineTranslator::Language::German:
        return QLocale::German;
    case OnlineTranslator::Language::Italian:
        return QLocale::Italian;
    case OnlineTranslator::Language::PortugueseBrazilian:
        return QLocale::Portuguese;
    case OnlineTranslator::Language::Russian:
        return QLocale::Russian;
    case OnlineTranslator::Language::Japanese:
        return QLocale::Japanese;
    case OnlineTranslator::Language::Korean:
        return QLocale::Korean;
    case OnlineTranslator::Language::ChineseSimplified:
        return QLocale::Chinese;
    case OnlineTranslator::Language::ChineseTraditional:
        return {QLocale::Chinese, QLocale::TraditionalChineseScript, QLocale::Taiwan};
    case OnlineTranslator::Language::Arabic:
        return QLocale::Arabic;
    case OnlineTranslator::Language::Hindi:
        return QLocale::Hindi;
    case OnlineTranslator::Language::Dutch:
        return QLocale::Dutch;
    case OnlineTranslator::Language::Polish:
        return QLocale::Polish;
    case OnlineTranslator::Language::Turkish:
        return QLocale::Turkish;
    case OnlineTranslator::Language::Vietnamese:
        return QLocale::Vietnamese;
    case OnlineTranslator::Language::Thai:
        return QLocale::Thai;
    case OnlineTranslator::Language::Czech:
        return QLocale::Czech;
    case OnlineTranslator::Language::Hungarian:
        return QLocale::Hungarian;
    default:
        return QLocale::English;
    }
}

void MozhiTTSProvider::updateState(QTextToSpeech::State newState)
{
    if (m_currentState != newState) {
        m_currentState = newState;
        emit stateChanged(newState);
    }
}

void MozhiTTSProvider::setError(QTextToSpeech::ErrorReason reason, const QString &message)
{
    m_errorReason = reason;
    m_errorString = message;
    updateState(QTextToSpeech::State::Error);
    emit errorOccurred(reason, message);
}

void MozhiTTSProvider::applyOptions(const ProviderOptions &options)
{
    if (options.hasOption("instance")) {
        setInstance(options.getOption("instance").toString());
    }

    if (options.hasOption("engine")) {
        bool ok = false;
        const int engineValue = options.getOption("engine").toInt(&ok);
        if (ok) {
            setEngine(static_cast<OnlineTranslator::Engine>(engineValue));
        }
    }
}

std::unique_ptr<ProviderOptions> MozhiTTSProvider::getDefaultOptions() const
{
    auto options = std::make_unique<ProviderOptions>();
    options->setOption("instance", "https://mozhi.aryak.me");
    options->setOption("engine", static_cast<int>(OnlineTranslator::Google));
    return options;
}

QStringList MozhiTTSProvider::getAvailableOptions() const
{
    return {"instance", "engine"};
}

ProviderUIRequirements MozhiTTSProvider::getUIRequirements() const
{
    ProviderUIRequirements requirements;
    requirements.requiredUIElements = {"engineComboBox"};
    requirements.supportedSignals = {};
    requirements.supportedCapabilities = {"engineSelection"};
    return requirements;
}

QStringList MozhiTTSProvider::availableSpeakers() const
{
    return {};
}

QStringList MozhiTTSProvider::availableSpeakersForVoice(const Voice &voice) const
{
    Q_UNUSED(voice);
    return availableSpeakers();
}

QString MozhiTTSProvider::currentSpeaker() const
{
    return {};
}

void MozhiTTSProvider::setSpeaker(const QString &speakerName)
{
    Q_UNUSED(speakerName)
}
