/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "hellotranslationplugin.h"

#include "hellotranslationprovider.h"

// Everything this backend can be set to, and where each value is kept.
//
// The second argument is the settings key, and leaving it empty is the point
// of this example: the backends shipped with Crow all name a key they were
// already writing before any of this existed, so that nobody's configuration
// file moved. A backend with no history has none to preserve, and gets
// Plugins/hello/prefix without having to invent a place to put it.
//
// The host reads these itself. There is no "read my settings" call to
// implement and no settings file to open - applyOptions() is handed the values
// and does not know where they came from.
QVector<ProviderOptionSpec> HelloTranslationPlugin::options() const
{
    return {
        makeOptionSpec(QStringLiteral("prefix"),
                       QString(),
                       QMetaType::QString,
                       QStringLiteral("[hello] "),
                       tr("Prefix"),
                       tr("Put in front of every line this backend hands back.")),
    };
}

// The plugin object outlives every provider it builds. Ownership of the
// provider is the caller's, through parent - do not keep a pointer to it.
ATranslationProvider *HelloTranslationPlugin::createProvider(QObject *parent)
{
    return new HelloTranslationProvider(parent);
}
