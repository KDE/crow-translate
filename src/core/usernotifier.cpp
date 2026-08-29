/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/usernotifier.h"

#include <QCoreApplication>
#include <QMetaObject>

UserNotifier::UserNotifier(QObject *parent)
    : QObject(parent)
{
}

UserNotifier *UserNotifier::instance()
{
    static UserNotifier notifier;
    return &notifier;
}

void UserNotifier::notify(const Notification &notification)
{
    // Queued, never direct. Two of the callers are static factory functions
    // that run while a frontend is still building itself, and one of those
    // used to paper over exactly that with QTimer::singleShot(100, ...) - a
    // guess at how long construction takes. A queued emission is the same
    // idea without the guess: it runs once the current call stack has
    // unwound, by which time the frontend that wants it is connected.
    UserNotifier *notifier = instance();
    QMetaObject::invokeMethod(
        notifier,
        [notifier, notification] {
            Q_EMIT notifier->notified(notification);
        },
        Qt::QueuedConnection);
}
