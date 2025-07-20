/*
 * SPDX-FileCopyrightText: 2025 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2025 Mauritius Clemens <spdx@janitor.chat>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef MEDIAPLAYER2____H
#define MEDIAPLAYER2____H

#include <QDebug>
#include <QMediaPlayer>
#include <QObject>

class PlaylistPlayer : public QMediaPlayer
{
    Q_OBJECT
public:
    explicit PlaylistPlayer(QObject *parent = nullptr);
    void addMedia(const QUrl &url);
    void addMedia(const QList<QUrl> &url);
    QList<QUrl> &getPlaylist();
    void setPlaylist(const QList<QUrl> &new_playlist);
    void next();
    void clearPlaylist();
    void playPlaylist();

private slots:
    void handleMediaStatus(PlaylistPlayer::MediaStatus status);

    void handleError(PlaylistPlayer::Error error, const QString &errorString);

private:
    QList<QUrl> playlist;
    int currentIndex = -1;
};

#endif // MEDIAPLAYER2____H
