/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef SETTINGSSTORE_H
#define SETTINGSSTORE_H

#include "crow_core_export.h"

#include <QString>

class QObject;
class QSettings;

// Where the application keeps its settings, for code that is not the
// application.
//
// AppSettings is a 471-line typed accessor over one QSettings, and it lives
// above the core: a backend cannot use it without depending on the window,
// the dialogs and the frontends. But a backend whose configuration is a tree
// rather than a list - LocalAI's per-endpoint URLs and models - has nothing a
// flat options schema can describe, and needs to read its own corner of the
// file.
//
// So the one thing that was genuinely shared, knowing *which* file it is,
// moves here. AppSettings opens it through this too, so there remains exactly
// one answer to that question, portable mode included.
namespace CrowSettings
{

// A QSettings on the application's own configuration. In a portable build
// that is settings.ini beside the executable when it exists, and the ordinary
// per-user location otherwise.
CROW_CORE_EXPORT QSettings *open(QObject *parent = nullptr);

#ifdef WITH_PORTABLE_MODE
// The INI beside the executable whose existence turns portable mode on.
CROW_CORE_EXPORT QString portableConfigName();
#endif

} // namespace CrowSettings

#endif // SETTINGSSTORE_H
