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

WaylandPortalScreenGrabber::WaylandPortalScreenGrabber(QObject *parent)
    : DBusScreenGrabber(parent)
    , m_interface(QStringLiteral("org.freedesktop.portal.Desktop"),
                  QStringLiteral("/org/freedesktop/portal/desktop"),
                  QStringLiteral("org.freedesktop.portal.Screenshot"))
{
}

bool WaylandPortalScreenGrabber::isAvailable()
{
    const QDBusInterface interface(QStringLiteral("org.freedesktop.portal.Desktop"),
                                   QStringLiteral("/org/freedesktop/portal/desktop"),
                                   QStringLiteral("org.freedesktop.portal.Screenshot"));
    return interface.isValid();
}

void WaylandPortalScreenGrabber::grab()
{
    auto *window = qobject_cast<QWidget *>(parent())->windowHandle();
    const QDBusPendingReply<QDBusObjectPath> reply = m_interface.asyncCall(QStringLiteral("Screenshot"), XdgDesktopPortal::parentWindow(window), QVariantMap());
    m_callWatcher = new QDBusPendingCallWatcher(reply, this);
    connect(m_callWatcher, &QDBusPendingCallWatcher::finished, [this] {
        const QDBusPendingReply<QDBusObjectPath> reply = readReply<QDBusObjectPath>();

        if (reply.isError()) {
            showError(reply.error().message());
            return;
        }

        m_responseServicePath = reply.value().path();
        const bool success = QDBusConnection::sessionBus().connect({},
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
    const QString path = response.value(QLatin1String("uri")).toUrl().toLocalFile();
    if (path.isEmpty()) {
        showError(tr("Received an empty path from xdg-desktop-portal."));
        return;
    }

    emit grabbed(splitScreenImages(path));
    QFile::remove(path);
}
