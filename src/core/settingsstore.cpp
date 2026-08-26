/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/settingsstore.h"

#include <QCoreApplication>
#include <QFile>
#include <QSettings>

#ifdef WITH_PORTABLE_MODE
QString CrowSettings::portableConfigName()
{
    // Initialize lazily because `QCoreApplication::applicationDirPath()` should be called after app creation
    static const QString portableConfigName = QCoreApplication::applicationDirPath() + "/" + QStringLiteral("settings.ini");
    return portableConfigName;
}
#endif

QSettings *CrowSettings::open(QObject *parent)
{
#ifdef WITH_PORTABLE_MODE
    if (QFile::exists(portableConfigName()))
        return new QSettings(portableConfigName(), QSettings::IniFormat, parent);
#endif
    return new QSettings(parent);
}
