/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef PLAYERBUTTONS_H
#define PLAYERBUTTONS_H

#include "onlinetranslator/onlinetranslator.h"
#include "onlinetranslator/onlinetts.h"

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

    QMediaPlayer *mediaPlayer() const;
    void setMediaPlayer(QMediaPlayer *mediaPlayer);
    QMediaPlaylist *playlist();

    void setSpeakShortcut(const QKeySequence &shortcut);
    QKeySequence speakShortcut() const;

    void speak(const QString &text, OnlineTranslator::Language lang, OnlineTranslator::Engine engine);
    void pauseSpeaking();
    void playPauseSpeaking();

public slots:
    void stopSpeaking();

signals:
    void playerMediaRequested();
    void stateChanged(QMediaPlayer::State state);
    void positionChanged(double progress);

private slots:
    void loadPlayerState(QMediaPlayer::State state);
    void onPlayPauseButtonPressed();
    void onPlayerPositionChanged(qint64 position);

private:
    static QMediaPlayer *s_currentlyPlaying;

    Ui::SpeakButtons *ui;
    QMediaPlayer *m_mediaPlayer = nullptr;
};

#endif // PLAYERBUTTONS_H
