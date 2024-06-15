/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "windowsautostartmanager.h"

#include <QCoreApplication>
#include <QDir>
#include <QSettings>

WindowsAutostartManager::WindowsAutostartManager(QObject *parent)
    : AbstractAutostartManager(parent)
{
}

bool WindowsAutostartManager::isAutostartEnabled() const
{
    QSettings autostartSettings(QStringLiteral("HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run"), QSettings::NativeFormat);
    return autostartSettings.contains(QStringLiteral("Crow Translate"));
}

void WindowsAutostartManager::setAutostartEnabled(bool enabled)
{
    QSettings autostartSettings(R"(HKEY_CURRENT_USER\SOFTWARE\Microsoft\Windows\CurrentVersion\Run)", QSettings::NativeFormat);
    if (enabled)
        autostartSettings.setValue("Crow Translate", QDir::toNativeSeparators(QCoreApplication::applicationFilePath()));
    else
        autostartSettings.remove("Crow Translate");
}
