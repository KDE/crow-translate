/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef ITRANSLATIONPLUGIN_H
#define ITRANSLATIONPLUGIN_H

#include "plugininterfaces/iproviderplugin.h"
#include "translator/atranslationprovider.h"

#include <QtPlugin>

// The version is part of the identifier on purpose: a plugin built against
// an older ATranslationProvider is not loaded at all, rather than loaded and
// then found to be missing a vtable entry.
#define CROW_TRANSLATION_PLUGIN_IID "org.kde.CrowTranslate.TranslationProvider/1.0"

// A translation backend, as something the application can be handed rather
// than something it has to name.
//
// Thin on purpose. ATranslationProvider already defines what a backend does;
// this only says how to get one, which is the single thing the three
// hardcoded registries were for.
class ITranslationPlugin : public IProviderPlugin
{
public:
    ~ITranslationPlugin() override = default;

    // The plugin object outlives every provider it builds; ownership of the
    // provider is the caller's, via parent.
    virtual ATranslationProvider *createProvider(QObject *parent) = 0;
};

Q_DECLARE_INTERFACE(ITranslationPlugin, CROW_TRANSLATION_PLUGIN_IID)

#endif // ITRANSLATIONPLUGIN_H
