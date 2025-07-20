/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef PLAYERSTOPPEDTRANSITION_H
#define PLAYERSTOPPEDTRANSITION_H

#include "playlistplayer.h"

#include <QSignalTransition>

class QMediaPlayer;

class PlayerStoppedTransition : public QSignalTransition
{
public:
    explicit PlayerStoppedTransition(PlaylistPlayer *player, QState *sourceState = nullptr);

protected:
    bool eventTest(QEvent *event) override;
};

#endif // PLAYERSTOPPEDTRANSITION_H
