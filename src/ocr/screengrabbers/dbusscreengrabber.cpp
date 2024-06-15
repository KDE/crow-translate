/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "dbusscreengrabber.h"

#include <QDBusPendingCall>
#include <QGuiApplication>
#include <QPixmap>
#include <QScreen>

DBusScreenGrabber::DBusScreenGrabber(QObject *parent)
    : AbstractScreenGrabber(parent)
{
}

// Split to separate images per screen
QMap<const QScreen *, QImage> DBusScreenGrabber::splitScreenImages(const QPixmap &pixmap)
{
    QMap<const QScreen *, QImage> images;
    for (QScreen *screen : QGuiApplication::screens()) {
        QRect geometry = screen->geometry();
        geometry.setSize(screen->size() * screen->devicePixelRatio());
        QPixmap screenPixmap = pixmap.copy(geometry);
        screenPixmap.setDevicePixelRatio(screen->devicePixelRatio());
        images.insert(screen, screenPixmap.toImage());
    }
    return images;
}

void DBusScreenGrabber::cancel()
{
    if (m_callWatcher != nullptr) {
        m_callWatcher->deleteLater();
        m_callWatcher = nullptr;
    }
}
