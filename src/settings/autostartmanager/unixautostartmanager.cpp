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

bool UnixAutostartManager::isAutostartEnabled() const
{
    return QFileInfo::exists(QStringLiteral("%1/autostart/%2").arg(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation), QGuiApplication::desktopFileName()));
}

void UnixAutostartManager::setAutostartEnabled(bool enabled)
{
    QDir autostartDir(QStringLiteral("%1/autostart").arg(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)));

    if (enabled) {
        // Create autorun file
        if (autostartDir.exists(QGuiApplication::desktopFileName()))
            return;

        if (!autostartDir.exists()) {
            if (!autostartDir.mkpath(QStringLiteral("."))) {
                showError(tr("Unable to create %1").arg(autostartDir.path()));
                return;
            }
        }

        const QString desktopFileName = QStringLiteral("/usr/share/applications/%1").arg(QGuiApplication::desktopFileName());
        if (!QFile::copy(desktopFileName, autostartDir.filePath(QGuiApplication::desktopFileName())))
            showError(tr("Unable to copy %1 to %2").arg(desktopFileName, autostartDir.path()));

    } else if (autostartDir.exists(QGuiApplication::desktopFileName())) {
        // Remove autorun file
        if (!autostartDir.remove(QGuiApplication::desktopFileName()))
            showError(tr("Unable to remove %1 from %2").arg(QGuiApplication::desktopFileName(), autostartDir.path()));
    }
}
