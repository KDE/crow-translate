/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "portalautostartmanager.h"

#include "xdgdesktopportal.h"
#include "settings/appsettings.h"

#include <QDBusReply>
#include <QWidget>
#include <QtCore>

PortalAutostartManager::PortalAutostartManager(QObject *parent)
    : AbstractAutostartManager(parent)
    , m_interface(QStringLiteral("org.freedesktop.portal.Desktop"),
                  QStringLiteral("/org/freedesktop/portal/desktop"),
                  QStringLiteral("org.freedesktop.portal.Background"))
{
}

bool PortalAutostartManager::isAutostartEnabled() const
{
    return AppSettings().isAutostartEnabled();
}

void PortalAutostartManager::setAutostartEnabled(bool enabled)
{
    auto *window = qobject_cast<QWidget *>(parent())->windowHandle();
    const QVariantMap options{
        {QStringLiteral("reason"), tr("Allow %1 to manage autostart setting for itself.").arg(QCoreApplication::applicationName())},
        {QStringLiteral("autostart"), enabled},
        {QStringLiteral("commandline"), QStringList{QCoreApplication::applicationFilePath()}},
        {QStringLiteral("dbus-activatable"), false},
    };
    const QDBusReply<QDBusObjectPath> reply = m_interface.call(QStringLiteral("RequestBackground"), XdgDesktopPortal::parentWindow(window), options);

    if (!reply.isValid()) {
        showError(reply.error().message());
        return;
    }

    const bool connected = m_interface.connection().connect(QStringLiteral("org.freedesktop.portal.Desktop"),
                                                            reply.value().path(),
                                                            QStringLiteral("org.freedesktop.portal.Request"),
                                                            QStringLiteral("Response"),
                                                            this,
                                                            SLOT(parsePortalResponse(quint32, QVariantMap)));
    if (!connected) {
        showError(tr("Unable to subscribe to response from xdg-desktop-portal."));
        return;
    }

    QEventLoop loop;
    connect(this, &PortalAutostartManager::responseParsed, &loop, &QEventLoop::quit);
    loop.exec();
}

bool PortalAutostartManager::isAvailable()
{
    QDBusInterface interface(QStringLiteral("org.freedesktop.portal.Desktop"),
                             QStringLiteral("/org/freedesktop/portal/desktop"),
                             QStringLiteral("org.freedesktop.portal.Background"));
    return QFile::exists(QStringLiteral("/.flatpak-info")) && interface.isValid();
}

void PortalAutostartManager::parsePortalResponse(quint32, const QVariantMap &results)
{
    AppSettings().setAutostartEnabled(results.value(QStringLiteral("autostart")).toBool());
    emit responseParsed();
}
