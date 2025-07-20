/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "playerstoppedtransition.h"

#include "playlistplayer.h"

#include <QStateMachine>

PlayerStoppedTransition::PlayerStoppedTransition(PlaylistPlayer *player, QState *sourceState)
    : QSignalTransition(player, &PlaylistPlayer::playbackStateChanged, sourceState)
{
}

bool PlayerStoppedTransition::eventTest(QEvent *event)
{
    if (!QSignalTransition::eventTest(event))
        return false;

    auto *signalEvent = dynamic_cast<QStateMachine::SignalEvent *>(event);
    return signalEvent->arguments().constFirst().toInt() == PlaylistPlayer::StoppedState;
}
