/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "waylandportalscreengrabber.h"

#include "xdgdesktopportal.h"

#include <QDBusInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusReply>
#include <QFile>
#include <QPixmap>
#include <QUrl>
#include <QWidget>

// https://github.com/flatpak/xdg-desktop-portal/blob/89d2197002f164d02d891c530dcaa2808f27f593/data/org.freedesktop.portal.Screenshot.xml
QDBusInterface WaylandPortalScreenGrabber::s_interface(QStringLiteral("org.freedesktop.portal.Desktop"),
                                                       QStringLiteral("/org/freedesktop/portal/desktop"),
                                                       QStringLiteral("org.freedesktop.portal.Screenshot"));

WaylandPortalScreenGrabber::WaylandPortalScreenGrabber(QObject *parent)
    : DBusScreenGrabber(parent)
{
}

bool WaylandPortalScreenGrabber::isAvailable()
{
    return s_interface.isValid();
}

void WaylandPortalScreenGrabber::grab()
{
    auto *window = qobject_cast<QWidget *>(parent())->windowHandle();
    const QDBusPendingReply<QDBusObjectPath> reply = s_interface.asyncCall(QStringLiteral("Screenshot"), XdgDesktopPortal::parentWindow(window), QVariantMap());
    m_callWatcher = new QDBusPendingCallWatcher(reply, this);
    connect(m_callWatcher, &QDBusPendingCallWatcher::finished, [this] {
        const QDBusPendingReply<QDBusObjectPath> reply = readReply<QDBusObjectPath>();

        if (reply.isError()) {
            showError(reply.error().message());
            return;
        }

        m_responseServicePath = reply.value().path();
        bool success = QDBusConnection::sessionBus().connect({},
                                                             m_responseServicePath,
                                                             QLatin1String("org.freedesktop.portal.Request"),
                                                             QLatin1String("Response"),
                                                             this,
                                                             SLOT(parsePortalResponse(uint, QVariantMap)));
        if (!success)
            showError(tr("Unable to subscribe to response from xdg-desktop-portal."));
    });
}

void WaylandPortalScreenGrabber::cancel()
{
    DBusScreenGrabber::cancel();

    QDBusConnection::sessionBus().disconnect({},
                                             m_responseServicePath,
                                             QLatin1String("org.freedesktop.portal.Request"),
                                             QLatin1String("Response"),
                                             this,
                                             SLOT(parsePortalResponse(uint, QVariantMap)));
}

void WaylandPortalScreenGrabber::parsePortalResponse(uint, const QVariantMap &response)
{
    QString path = response.value(QLatin1String("uri")).toUrl().toLocalFile();
    if (path.isEmpty()) {
        showError(tr("Received an empty path from xdg-desktop-portal."));
        return;
    }

    emit grabbed(splitScreenImages(path));
    QFile::remove(path);
}
