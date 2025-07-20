/*
 * SPDX-FileCopyrightText: 2025 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2025 Mauritius Clemens <spdx@janitor.chat>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "playlistplayer.h"

PlaylistPlayer::PlaylistPlayer(QObject *parent)
    : QMediaPlayer{parent}
    , currentIndex(-1)
{
    connect(this, &PlaylistPlayer::mediaStatusChanged,
            this, &PlaylistPlayer::handleMediaStatus);
    // Connect error signal for error handling
    connect(this, &PlaylistPlayer::errorOccurred,
            this, &PlaylistPlayer::handleError);
}

void PlaylistPlayer::addMedia(const QUrl &url)
{
    playlist.append(url);
}
void PlaylistPlayer::addMedia(const QList<QUrl> &otherlist)
{
    playlist += otherlist;
}
QList<QUrl> &PlaylistPlayer::getPlaylist()
{
    return playlist;
}
void PlaylistPlayer::setPlaylist(const QList<QUrl> &new_playlist)
{
    playlist = new_playlist;
}
void PlaylistPlayer::playPlaylist()
{
    if (playlist.isEmpty()) {
        qDebug() << "Playlist is empty";
        return;
    }
    currentIndex = 0;
    setSource(playlist.first());
    play();
}
void PlaylistPlayer::clearPlaylist()
{
    stop();
    playlist.clear();
    currentIndex = -1;
}
void PlaylistPlayer::next()
{
    if (playlist.isEmpty())
        return;

    currentIndex = (currentIndex + 1) % playlist.size();
    setSource(playlist.at(currentIndex));
    play();
}
void PlaylistPlayer::handleMediaStatus(PlaylistPlayer::MediaStatus status)
{
    if (status == PlaylistPlayer::EndOfMedia && currentIndex >= 0 && currentIndex < playlist.size()) {
        next();
    } else {
        currentIndex = -1;
    }
}

// Skip track on error and continue playback
void PlaylistPlayer::handleError(PlaylistPlayer::Error error, const QString &errorString)
{
    Q_UNUSED(error);
    qWarning() << "Media error: " << errorString;
    next(); // Skip to next track on error
}
