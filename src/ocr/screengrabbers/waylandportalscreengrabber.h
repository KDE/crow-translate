/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef WAYLANDPORTALSCREENGRABBER_H
#define WAYLANDPORTALSCREENGRABBER_H

#include "dbusscreengrabber.h"

#include <QDBusInterface>

class WaylandPortalScreenGrabber : public DBusScreenGrabber
{
    Q_OBJECT
    Q_DISABLE_COPY(WaylandPortalScreenGrabber)

public:
    explicit WaylandPortalScreenGrabber(QObject *parent = nullptr);

    static bool isAvailable();

public slots:
    void grab() override;
    void cancel() override;

private slots:
    void parsePortalResponse(uint, const QVariantMap &response);

private:
    QString m_responseServicePath;

    static QDBusInterface s_interface;
};

#endif // WAYLANDPORTALSCREENGRABBER_H
