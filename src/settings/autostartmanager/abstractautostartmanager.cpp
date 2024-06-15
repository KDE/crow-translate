/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "abstractautostartmanager.h"
#if defined(Q_OS_UNIX)
#include "portalautostartmanager.h"
#include "unixautostartmanager.h"
#elif defined(Q_OS_DARWIN)
#include "macosautostartmanager.h"
#elif defined(Q_OS_WIN)
#include "windowsautostartmanager.h"
#endif

#include <QMessageBox>

AbstractAutostartManager::AbstractAutostartManager(QObject *parent)
    : QObject(parent)
{
}

AbstractAutostartManager *AbstractAutostartManager::createAutostartManager(QObject *parent)
{
#if defined(Q_OS_UNIX)
    if (PortalAutostartManager::isAvailable())
        return new PortalAutostartManager(parent);
    return new UnixAutostartManager(parent);
#elif defined(Q_OS_DARWIN)
    return new macOSAutostartManager(parent);
#elif defined(Q_OS_WIN)
    return new WindowsAutostartManager(parent);
#else
    qFatal("No autostart provider implemented");
#endif
}

void AbstractAutostartManager::showError(const QString &informativeText)
{
    QMessageBox message;
    message.setIcon(QMessageBox::Critical);
    message.setText(tr("Unable to apply autostart settings"));
    message.setInformativeText(informativeText);
    message.exec();
}
