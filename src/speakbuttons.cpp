/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "speakbuttons.h"
#include "ui_speakbuttons.h"

#include "settings/appsettings.h"

#include <QMediaPlaylist>
#include <QMessageBox>

QMediaPlayer *SpeakButtons::s_currentlyPlaying = nullptr;

SpeakButtons::SpeakButtons(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::SpeakButtons)
{
    ui->setupUi(this);

    connect(ui->playPauseButton, &QAbstractButton::clicked, this, &SpeakButtons::onPlayPauseButtonPressed);
    connect(ui->stopButton, &QAbstractButton::clicked, this, &SpeakButtons::stopSpeaking);
}

SpeakButtons::~SpeakButtons()
{
    delete ui;
}

QMediaPlayer *SpeakButtons::mediaPlayer() const
{
    return m_mediaPlayer;
}

void SpeakButtons::setMediaPlayer(QMediaPlayer *mediaPlayer)
{
    if (m_mediaPlayer != nullptr) {
        disconnect(m_mediaPlayer, &QMediaPlayer::stateChanged, this, &SpeakButtons::loadPlayerState);
        disconnect(m_mediaPlayer, &QMediaPlayer::stateChanged, this, &SpeakButtons::stateChanged);
        disconnect(m_mediaPlayer, &QMediaPlayer::positionChanged, this, &SpeakButtons::onPlayerPositionChanged);
    }

    m_mediaPlayer = mediaPlayer;
    if (m_mediaPlayer->playlist() == nullptr)
        m_mediaPlayer->setPlaylist(new QMediaPlaylist);

    connect(m_mediaPlayer, &QMediaPlayer::positionChanged, this, &SpeakButtons::onPlayerPositionChanged);
    connect(m_mediaPlayer, &QMediaPlayer::stateChanged, this, &SpeakButtons::loadPlayerState);
    connect(m_mediaPlayer, &QMediaPlayer::stateChanged, this, &SpeakButtons::stateChanged);

    loadPlayerState(m_mediaPlayer->state());
}

QMediaPlaylist *SpeakButtons::playlist()
{
    return m_mediaPlayer->playlist();
}

void SpeakButtons::setSpeakShortcut(const QKeySequence &shortcut)
{
    ui->playPauseButton->setShortcut(shortcut);
}

QKeySequence SpeakButtons::speakShortcut() const
{
    return ui->playPauseButton->shortcut();
}

OnlineTts::Voice SpeakButtons::voice(OnlineTranslator::Engine engine) const
{
    switch (engine) {
    case OnlineTranslator::Yandex:
        return m_yandexVoice;
    default:
        return OnlineTts::NoVoice;
    }
}

void SpeakButtons::setVoice(OnlineTranslator::Engine engine, OnlineTts::Voice voice)
{
    switch (engine) {
    case OnlineTranslator::Yandex:
        m_yandexVoice = voice;
        break;
    default:
        break;
    }
}

OnlineTts::Emotion SpeakButtons::emotion(OnlineTranslator::Engine engine) const
{
    switch (engine) {
    case OnlineTranslator::Yandex:
        return m_yandexEmotion;
    default:
        return OnlineTts::NoEmotion;
    }
}

void SpeakButtons::setEmotion(OnlineTranslator::Engine engine, OnlineTts::Emotion emotion)
{
    switch (engine) {
    case OnlineTranslator::Yandex:
        m_yandexEmotion = emotion;
        break;
    default:
        break;
    }
}

QMap<OnlineTranslator::Language, QLocale::Country> SpeakButtons::regions(OnlineTranslator::Engine engine) const
{
    switch (engine) {
    case OnlineTranslator::Google:
        return m_googleRegions;
    default:
        return {};
    }
}

void SpeakButtons::setRegions(OnlineTranslator::Engine engine, QMap<OnlineTranslator::Language, QLocale::Country> regions)
{
    switch (engine) {
    case OnlineTranslator::Google:
        m_googleRegions = std::move(regions);
        break;
    default:
        break;
    }
}

void SpeakButtons::speak(const QString &text, OnlineTranslator::Language lang, OnlineTranslator::Engine engine)
{
    if (text.isEmpty()) {
        QMessageBox::information(this, tr("No text specified"), tr("Playback text is empty"));
        return;
    }

    OnlineTts onlineTts;
    onlineTts.setRegions(m_googleRegions);

    onlineTts.generateUrls(text, engine, lang, voice(engine), emotion(engine));
    if (onlineTts.error() != OnlineTts::NoError) {
        QMessageBox::critical(this, tr("Unable to generate URLs for TTS"), onlineTts.errorString());
        return;
    }

    // Use playlist to split long queries due engines limit
    const QList<QMediaContent> media = onlineTts.media();
    playlist()->clear();
    playlist()->addMedia(media);
    m_mediaPlayer->play();
}

void SpeakButtons::pauseSpeaking()
{
    m_mediaPlayer->pause();
}

void SpeakButtons::playPauseSpeaking()
{
    if (m_mediaPlayer->state() == QMediaPlayer::PlayingState)
        m_mediaPlayer->pause();
    else
        m_mediaPlayer->play();
}

void SpeakButtons::stopSpeaking()
{
    m_mediaPlayer->stop();
}

void SpeakButtons::loadPlayerState(QMediaPlayer::State state)
{
    switch (state) {
    case QMediaPlayer::StoppedState:
        if (s_currentlyPlaying == m_mediaPlayer)
            s_currentlyPlaying = nullptr;

        ui->playPauseButton->setIcon(QIcon::fromTheme(QStringLiteral("media-playback-start")));
        ui->stopButton->setEnabled(false);
        break;
    case QMediaPlayer::PlayingState:
        if (s_currentlyPlaying != nullptr)
            s_currentlyPlaying->pause();
        s_currentlyPlaying = m_mediaPlayer;

        ui->playPauseButton->setIcon(QIcon::fromTheme(QStringLiteral("media-playback-pause")));
        ui->stopButton->setEnabled(true);
        break;
    case QMediaPlayer::PausedState:
        if (s_currentlyPlaying == m_mediaPlayer)
            s_currentlyPlaying = nullptr;

        ui->playPauseButton->setIcon(QIcon::fromTheme(QStringLiteral("media-playback-start")));
        break;
    }
}

void SpeakButtons::onPlayPauseButtonPressed()
{
    if (m_mediaPlayer->state() == QMediaPlayer::StoppedState)
        emit playerMediaRequested();
    else
        playPauseSpeaking();
}

void SpeakButtons::onPlayerPositionChanged(qint64 position)
{
    if (m_mediaPlayer->duration() != 0)
        emit positionChanged(static_cast<double>(position) / static_cast<double>(m_mediaPlayer->duration()));
    else
        emit positionChanged(0);
}
