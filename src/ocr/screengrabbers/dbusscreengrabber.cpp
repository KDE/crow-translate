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
