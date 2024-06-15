/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "xdgdesktopportal.h"

#ifdef WITH_KWAYLAND
#include "waylandhelper.h"
#endif

#include <QDebug>
#include <QWindow>
#include <QX11Info>

QString XdgDesktopPortal::parentWindow(QWindow *activeWindow)
{
    if (!QX11Info::isPlatformX11()) {
#ifdef WITH_KWAYLAND
        WaylandHelper wayland(activeWindow);
        QEventLoop loop;
        QObject::connect(&wayland, &WaylandHelper::xdgExportDone, &loop, &QEventLoop::quit);
        loop.exec();
        const QString handle = wayland.exportedHandle();
        if (handle.isEmpty()) {
            return {};
        }
        return QStringLiteral("wayland:%1").arg(handle);
#else
        return {};
#endif
    }

    return QStringLiteral("x11:%1").arg(activeWindow->winId(), 0, 16);
}
