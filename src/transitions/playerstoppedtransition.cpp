/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "playerstoppedtransition.h"

#include <QMediaPlayer>
#include <QStateMachine>

PlayerStoppedTransition::PlayerStoppedTransition(QMediaPlayer *player, QState *sourceState)
    : QSignalTransition(player, &QMediaPlayer::stateChanged, sourceState)
{
}

bool PlayerStoppedTransition::eventTest(QEvent *event)
{
    if (!QSignalTransition::eventTest(event))
        return false;

    auto *signalEvent = dynamic_cast<QStateMachine::SignalEvent *>(event);
    return signalEvent->arguments().constFirst().toInt() == QMediaPlayer::StoppedState;
}
