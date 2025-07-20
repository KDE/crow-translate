/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef PLAYERBUTTONS_H
#define PLAYERBUTTONS_H

#include "onlinetranslator.h"
#include "playlistplayer.h"

#include <QMediaPlayer>
#include <QWidget>

class AppSettings;

namespace Ui
{
class SpeakButtons;
}

class SpeakButtons : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY(SpeakButtons)

public:
    explicit SpeakButtons(QWidget *parent = nullptr);
    ~SpeakButtons() override;

    PlaylistPlayer *mediaPlayer() const;
    void setMediaPlayer(PlaylistPlayer *mediaPlayer);
    QList<QUrl> &playlist();

    void setSpeakShortcut(const QKeySequence &shortcut);
    QKeySequence speakShortcut() const;

    void speak(OnlineTranslator &translator, const QString &text, OnlineTranslator::Language lang, OnlineTranslator::Engine engine);
    void pauseSpeaking();
    void playPauseSpeaking();

public slots:
    void stopSpeaking();

signals:
    void playerMediaRequested();
    void stateChanged(QMediaPlayer::PlaybackState state);
    void positionChanged(double progress);

private slots:
    void loadPlayerState(QMediaPlayer::PlaybackState state);
    void onPlayPauseButtonPressed();
    void onPlayerPositionChanged(qint64 position);

private:
    static PlaylistPlayer *s_currentlyPlaying;

    Ui::SpeakButtons *ui;
    PlaylistPlayer *m_mediaPlayer = nullptr;
};

#endif // PLAYERBUTTONS_H
