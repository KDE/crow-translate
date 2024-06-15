/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "waylandhelper.h"

#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/surface.h>
#include <KWayland/Client/xdgforeign.h>

using namespace KWayland::Client;

WaylandHelper::WaylandHelper(QWindow *parent)
    : QObject(parent)
    , surface(Surface::fromWindow(parent))
    , registry(new Registry(this))
    , connection(ConnectionThread::fromApplication(this))
{
    if (!surface->isValid())
        qFatal("Invalid Wayland surface");
    registry.create(connection);
    registry.setup();
    if (!registry.isValid())
        qFatal("Invalid Wayland registry");
    connect(connection, &ConnectionThread::connectionDied, &registry, &Registry::destroy);
    connect(&registry, &Registry::exporterUnstableV2Announced, this, &WaylandHelper::onXdgExporterAnnounced);
    connect(&registry, &Registry::interfacesAnnounced, this, &WaylandHelper::onInterfacesAnnounced);
}

void WaylandHelper::onXdgExporterAnnounced(quint32 name, quint32 version)
{
    auto *xdgExporter = registry.createXdgExporter(name, version, this);
    if (!xdgExporter->isValid())
        qFatal("Invalid Wayland XdgExporter");
    xdgExportedTopLevel = xdgExporter->exportTopLevel(surface, this);
    if (!xdgExportedTopLevel->isValid())
        qFatal("Invalid Wayland toplevel export");
    connect(xdgExportedTopLevel, &XdgExported::done, this, &WaylandHelper::xdgExportDone);
}

void WaylandHelper::onInterfacesAnnounced()
{
    if (!xdgExportedTopLevel) {
        qWarning("Wayland compositor didn't announce XdgExporter, can't get export handle");
        emit xdgExportDone();
    }
}

QString WaylandHelper::exportedHandle() const
{
    if (!xdgExportedTopLevel)
        return {};
    return xdgExportedTopLevel->handle();
}
