/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef WAYLANDHELPER_H
#define WAYLANDHELPER_H

#include <QtCore>

#include <KWayland/Client/registry.h>

namespace KWayland::Client
{
class Surface;
class ConnectionThread;
class XdgExporter;
class XdgExported;
}
class QWindow;

class WaylandHelper : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(WaylandHelper)

public:
    explicit WaylandHelper(QWindow *parent);

    QString exportedHandle() const;

signals:
    void xdgExportDone();

private:
    KWayland::Client::Surface *surface;
    KWayland::Client::Registry registry;
    KWayland::Client::ConnectionThread *connection;
    KWayland::Client::XdgExported *xdgExportedTopLevel = nullptr;

private slots:
    void onXdgExporterAnnounced(quint32 name, quint32 version);
    void onInterfacesAnnounced();
};

#endif // WAYLANDHELPER_H
