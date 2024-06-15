/*
 *  Copyright © 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 *  Copyright © 2022 Volk Milit <javirrdar@gmail.com>
 *
 *  This file is part of Crow Translate.
 *
 *  Crow Translate is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Crow Translate is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Crow Translate. If not, see <https://www.gnu.org/licenses/>.
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
