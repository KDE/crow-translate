/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "waylandgnomescreengrabber.h"

#include <QCoreApplication>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDir>
#include <QFile>
#include <QPixmap>

const QString WaylandGnomeScreenGrabber::s_fileName = QDir::temp().filePath(QStringLiteral("ocr-screenshot.png"));

// https://github.com/GNOME/gnome-shell/blob/7a57528bd7940e68c404d15d398f88730821cec9/data/dbus-interfaces/org.gnome.Shell.Screenshot.xml
QDBusInterface WaylandGnomeScreenGrabber::s_interface(QStringLiteral("org.gnome.Shell"),
                                                      QStringLiteral("/org/gnome/Shell/Screenshot"),
                                                      QStringLiteral("org.gnome.Shell.Screenshot"));

WaylandGnomeScreenGrabber::WaylandGnomeScreenGrabber(QObject *parent)
    : DBusScreenGrabber(parent)
{
}

bool WaylandGnomeScreenGrabber::isAvailable()
{
    return s_interface.isValid();
}

void WaylandGnomeScreenGrabber::grab()
{
    const QDBusPendingReply<bool> reply = s_interface.asyncCall(QStringLiteral("Screenshot"), false, false, s_fileName);
    m_callWatcher = new QDBusPendingCallWatcher(reply, this);
    connect(m_callWatcher, &QDBusPendingCallWatcher::finished, [this] {
        const QDBusPendingReply<bool> reply = readReply<bool>();

        if (!reply.isValid()) {
            showError(reply.error().message());
            return;
        }

        if (!reply.value()) {
            showError(tr("GNOME failed to take screenshot."));
            return;
        }

        emit grabbed(splitScreenImages(s_fileName));
        QFile::remove(s_fileName);
    });
}
