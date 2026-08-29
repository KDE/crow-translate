/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "abstractscreengrabber.h"

#include "genericscreengrabber.h"
#include "core/usernotifier.h"

#include <QGuiApplication>

#ifdef Q_OS_LINUX
#include "waylandgnomescreengrabber.h"
#include "waylandplasmascreengrabber.h"
#include "waylandportalscreengrabber.h"

#endif

AbstractScreenGrabber::AbstractScreenGrabber(QObject *parent)
    : QObject(parent)
{
}

AbstractScreenGrabber *AbstractScreenGrabber::createScreenGrabber(QObject *parent)
{
#ifdef Q_OS_LINUX
    if (qGuiApp->nativeInterface<QNativeInterface::QX11Application>() == nullptr) {
        if (WaylandGnomeScreenGrabber::isAvailable())
            return new WaylandGnomeScreenGrabber(parent);
        if (WaylandPlasmaScreenGrabber::isAvailable())
            return new WaylandPlasmaScreenGrabber(parent);
        if (WaylandPortalScreenGrabber::isAvailable())
            return new WaylandPortalScreenGrabber(parent);
    }
#endif
    return new GenericScreenGrabber(parent);
}

void AbstractScreenGrabber::showError(const QString &errorString)
{
    UserNotifier::Notification notification;
    notification.severity = UserNotifier::Severity::Critical;
    notification.title = tr("Unable to grab screen");
    notification.text = tr("Unable to grab screen");
    notification.details = errorString;
    UserNotifier::notify(notification);

    // Emitted straight away now. It used to wait for the user to dismiss a
    // modal dialog first, which held the failure back from everything that
    // reacts to it - the status strip went on showing a capture in progress
    // for as long as the box stayed up.
    emit grabbingFailed();
}
