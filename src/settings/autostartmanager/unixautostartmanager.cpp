/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "unixautostartmanager.h"

#include <QDir>
#include <QGuiApplication>
#include <QStandardPaths>

UnixAutostartManager::UnixAutostartManager(QObject *parent)
    : AbstractAutostartManager(parent)
{
}

QString UnixAutostartManager::desktopFileFullName() const
{
    return QGuiApplication::desktopFileName() + ".desktop";
}

bool UnixAutostartManager::isAutostartEnabled() const
{
    return QFileInfo::exists(QStringLiteral("%1/autostart/%2").arg(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation), desktopFileFullName()));
}

void UnixAutostartManager::setAutostartEnabled(bool enabled)
{
    QDir autostartDir(QStringLiteral("%1/autostart").arg(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)));

    if (enabled) {
        // Create autorun file
        if (autostartDir.exists(desktopFileFullName()))
            return;

        if (!autostartDir.exists()) {
            if (!autostartDir.mkpath(QStringLiteral("."))) {
                showError(tr("Unable to create %1").arg(autostartDir.path()));
                return;
            }
        }

        // The desktop file is where the application was installed, and that
        // is found the way the desktop finds it: along the XDG data
        // directories. Only /usr was looked in, so autostart could not be
        // switched on for an installation under /usr/local or /opt, in a Nix
        // profile, from an AppImage or out of a build tree. /usr stays the
        // last resort, and the place the message names when there is none.
        QString desktopFileName = QStandardPaths::locate(QStandardPaths::ApplicationsLocation, desktopFileFullName());
        if (desktopFileName.isEmpty()) {
            desktopFileName = QStringLiteral("/usr/share/applications/%1").arg(desktopFileFullName());
        }
        if (!QFile::copy(desktopFileName, autostartDir.filePath(desktopFileFullName())))
            showError(tr("Unable to copy %1 to %2").arg(desktopFileName, autostartDir.path()));

    } else if (autostartDir.exists(desktopFileFullName())) {
        // Remove autorun file
        if (!autostartDir.remove(desktopFileFullName()))
            showError(tr("Unable to remove %1 from %2").arg(desktopFileFullName(), autostartDir.path()));
    }
}
