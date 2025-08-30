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
    qDebug() << "PlaylistPlayer::setPlaylist() - Set playlist with" << playlist.size() << "URLs";
    for (int i = 0; i < playlist.size(); ++i) {
        qDebug() << "PlaylistPlayer::setPlaylist() - URL" << i << ":" << playlist[i].toString();
    }
}
void PlaylistPlayer::playPlaylist()
{
    if (playlist.isEmpty()) {
        qDebug() << "PlaylistPlayer::playPlaylist() - Playlist is empty";
        return;
    }
    currentIndex = 0;
    const QUrl &firstUrl = playlist.first();
    qDebug() << "PlaylistPlayer::playPlaylist() - Starting playback with URL:" << firstUrl.toString();
    qDebug() << "PlaylistPlayer::playPlaylist() - URL valid:" << firstUrl.isValid() << "scheme:" << firstUrl.scheme();
    setSource(firstUrl);
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
    const QUrl &nextUrl = playlist.at(currentIndex);
    qDebug() << "PlaylistPlayer::next() - Moving to URL" << currentIndex << ":" << nextUrl.toString();
    qDebug() << "PlaylistPlayer::next() - URL valid:" << nextUrl.isValid() << "scheme:" << nextUrl.scheme();
    setSource(nextUrl);
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
    QString enhancedErrorString = errorString;

    // Enhance error message for HTTP errors
    if (errorString.contains("Server returned 5XX Server Error")) {
        enhancedErrorString = "HTTP 500 Internal Server Error";
    } else if (errorString.contains("Server returned 4XX Client Error")) {
        enhancedErrorString = "HTTP 4XX Client Error";
    }

    qWarning() << "PlaylistPlayer::handleError() - Error:" << error << "Enhanced string:" << enhancedErrorString;
    qWarning() << "PlaylistPlayer::handleError() - Original string:" << errorString;
    qWarning() << "PlaylistPlayer::handleError() - Current URL index:" << currentIndex;
    if (currentIndex >= 0 && currentIndex < playlist.size()) {
        qWarning() << "PlaylistPlayer::handleError() - Failed URL:" << playlist[currentIndex].toString();
    }
    qWarning() << "PlaylistPlayer::handleError() - Current source:" << source().toString();

    next(); // Skip to next track on error
}
