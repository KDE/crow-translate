/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "speakbuttons.h"
#include "ui_speakbuttons.h"

#include "onlinetranslator.h"

#include <QMessageBox>

PlaylistPlayer *SpeakButtons::s_currentlyPlaying = nullptr;

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

PlaylistPlayer *SpeakButtons::mediaPlayer() const
{
    return m_mediaPlayer;
}

void SpeakButtons::setMediaPlayer(PlaylistPlayer *mediaPlayer)
{
    if (m_mediaPlayer != nullptr) {
        disconnect(m_mediaPlayer, &PlaylistPlayer::playbackStateChanged, this, &SpeakButtons::loadPlayerState);
        disconnect(m_mediaPlayer, &PlaylistPlayer::playbackStateChanged, this, &SpeakButtons::stateChanged);
        disconnect(m_mediaPlayer, &PlaylistPlayer::positionChanged, this, &SpeakButtons::onPlayerPositionChanged);
    }
    m_mediaPlayer = mediaPlayer;

    if (mediaPlayer != nullptr) {
        connect(m_mediaPlayer, &PlaylistPlayer::positionChanged, this, &SpeakButtons::onPlayerPositionChanged);
        connect(m_mediaPlayer, &PlaylistPlayer::playbackStateChanged, this, &SpeakButtons::loadPlayerState);
        connect(m_mediaPlayer, &PlaylistPlayer::playbackStateChanged, this, &SpeakButtons::stateChanged);

        loadPlayerState(m_mediaPlayer->playbackState());
    } else {
        ui->playPauseButton->setIcon(QIcon::fromTheme(QStringLiteral("media-playback-start")));
        ui->stopButton->setEnabled(false);
        if (s_currentlyPlaying != nullptr) {
            s_currentlyPlaying->stop();
            s_currentlyPlaying = nullptr;
        }
    }
}

QList<QUrl> &SpeakButtons::playlist()
{
    return m_mediaPlayer->getPlaylist();
}

void SpeakButtons::setSpeakShortcut(const QKeySequence &shortcut)
{
    ui->playPauseButton->setShortcut(shortcut);
}

QKeySequence SpeakButtons::speakShortcut() const
{
    return ui->playPauseButton->shortcut();
}

void SpeakButtons::speak(OnlineTranslator &translator, const QString &text, OnlineTranslator::Language lang, OnlineTranslator::Engine engine)
{
    if (text.isEmpty()) {
        QMessageBox::information(this, tr("No text specified"), tr("Playback text is empty"));
        return;
    }

    const QList<QUrl> media = translator.generateUrls(text, engine, lang);
    if (translator.error() != OnlineTranslator::NoError) {
        QMessageBox::critical(this, tr("Unable to generate URLs for TTS"), translator.errorString());
        return;
    }

    // Use playlist to split long queries due engines limit
    m_mediaPlayer->clearPlaylist();
    m_mediaPlayer->addMedia(media);
    m_mediaPlayer->playPlaylist();
}

void SpeakButtons::pauseSpeaking()
{
    m_mediaPlayer->pause();
}

void SpeakButtons::playPauseSpeaking()
{
    if (m_mediaPlayer->playbackState() == PlaylistPlayer::PlayingState)
        m_mediaPlayer->pause();
    else
        m_mediaPlayer->play();
}

void SpeakButtons::stopSpeaking()
{
    m_mediaPlayer->stop();
}

void SpeakButtons::loadPlayerState(PlaylistPlayer::PlaybackState state)
{
    switch (state) {
    case PlaylistPlayer::StoppedState:
        if (s_currentlyPlaying == m_mediaPlayer)
            s_currentlyPlaying = nullptr;

        ui->playPauseButton->setIcon(QIcon::fromTheme(QStringLiteral("media-playback-start")));
        ui->stopButton->setEnabled(false);
        break;
    case PlaylistPlayer::PlayingState:
        if (s_currentlyPlaying != nullptr)
            s_currentlyPlaying->pause();
        s_currentlyPlaying = m_mediaPlayer;

        ui->playPauseButton->setIcon(QIcon::fromTheme(QStringLiteral("media-playback-pause")));
        ui->stopButton->setEnabled(true);
        break;
    case PlaylistPlayer::PausedState:
        if (s_currentlyPlaying == m_mediaPlayer)
            s_currentlyPlaying = nullptr;

        ui->playPauseButton->setIcon(QIcon::fromTheme(QStringLiteral("media-playback-start")));
        break;
    }
}

void SpeakButtons::onPlayPauseButtonPressed()
{
    if (m_mediaPlayer->playbackState() == PlaylistPlayer::StoppedState)
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
