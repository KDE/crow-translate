/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef WAYLANDGNOMESCREENGRABBER_H
#define WAYLANDGNOMESCREENGRABBER_H

#include "dbusscreengrabber.h"

#include <QDBusInterface>

class WaylandGnomeScreenGrabber : public DBusScreenGrabber
{
    Q_OBJECT
    Q_DISABLE_COPY(WaylandGnomeScreenGrabber)

public:
    explicit WaylandGnomeScreenGrabber(QObject *parent = nullptr);

    static bool isAvailable();

public slots:
    void grab() override;

private:
    static const QString s_fileName;
    // https://gitlab.gnome.org/GNOME/gnome-shell/-/blob/main/data/dbus-interfaces/org.gnome.Shell.Screenshot.xml
    QDBusInterface m_interface;
};

#endif // WAYLANDGNOMESCREENGRABBER_H
