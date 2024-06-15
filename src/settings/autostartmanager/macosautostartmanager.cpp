/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "macosautostartmanager.h"

#include "cmake.h"

#include <QApplication>
#include <QtCore>

macOSAutostartManager::macOSAutostartManager(QObject *parent)
    : AbstractAutostartManager(parent)
{
}

QString macOSAutostartManager::getLaunchAgentFilename()
{
    QDir launchAgentDir = QDir(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + QStringLiteral("/../LaunchAgents"));
    return QFile(launchAgentDir.absoluteFilePath(QStringLiteral(APPLICATION_ID).append(".plist"))).fileName();
}

bool macOSAutostartManager::isAutostartEnabled() const
{
    return QFile(getLaunchAgentFilename()).exists();
}

void macOSAutostartManager::setAutostartEnabled(bool enabled)
{
    const bool autostartEnabled = isAutostartEnabled();
    if (enabled && !autostartEnabled) {
        QSettings launchAgent(getLaunchAgentFilename(), QSettings::NativeFormat);
        launchAgent.setValue("Label", QStringLiteral(APPLICATION_ID));
        launchAgent.setValue("ProgramArguments", QStringList() << QApplication::applicationFilePath());
        launchAgent.setValue("RunAtLoad", true);
        launchAgent.setValue("StandardErrorPath", "/dev/null");
        launchAgent.setValue("StandardOutPath", "/dev/null");
    } else if (autostartEnabled) {
        QFile::remove(getLaunchAgentFilename());
    }
}
