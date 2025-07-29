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

    QFuture<void> m_readImageFuture;
    QDBusInterface m_interface;
};

#endif // WAYLANDPLASMASCREENGRABBER_H
