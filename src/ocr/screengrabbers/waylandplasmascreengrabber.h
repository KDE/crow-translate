/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef WAYLANDPLASMASCREENGRABBER_H
#define WAYLANDPLASMASCREENGRABBER_H

#include "dbusscreengrabber.h"

#include <QDBusInterface>
#include <QFuture>
#include <QImage>
#include <QPoint>

class WaylandPlasmaScreenGrabber : public DBusScreenGrabber
{
    Q_OBJECT
    Q_DISABLE_COPY(WaylandPlasmaScreenGrabber)

public:
    explicit WaylandPlasmaScreenGrabber(QObject *parent = nullptr);

    static bool isAvailable();

public slots:
    void grab() override;
    void cancel() override;

private:
    void readPixmapFromSocket(int socketDescriptor, QImage::Format format);
    void detectScreenFromWorkspace(const QImage &workspaceImage, const QPoint &cursorPos);
    void showPermissionWarning();
    void fallbackToCaptureInteractive();
    uint getInterfaceVersion();

    QFuture<void> m_readImageFuture;
    //  https://invent.kde.org/plasma/kwin/-/blob/master/src/plugins/screenshot/org.kde.KWin.ScreenShot2.xml?ref_type=heads
    QDBusInterface m_interface;
    uint m_version = 0;
};

#endif // WAYLANDPLASMASCREENGRABBER_H
