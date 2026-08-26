/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef IPROVIDERPLUGIN_H
#define IPROVIDERPLUGIN_H

#include "provideroptions.h"
#include "plugininterfaces/provideroptionspec.h"

#include <QString>
#include <QVector>

// Which of the three interchangeable things a plugin is one of.
//
// Not a property of the interface - a plugin declares it in its metadata so
// the host can tell what it found without loading it, and so plugins can be
// installed one directory per family.
enum class PluginFamily : quint8 {
    Translation,
    Tts,
    Ocr
};

// What a plugin is, told to a host that has never heard of it.
//
// Assembled by PluginManager rather than returned by the plugin: the identity
// half comes from the module's own JSON metadata, which QPluginLoader can read
// without dlopening anything, so a settings dialog can list every installed
// backend without building one. The options come from the live object.
//
// Splitting it that way is what keeps the two halves from being written twice
// and drifting.
struct PluginInfo {
    // Stable, lowercase, and written into the settings file. Renaming one
    // silently deselects it for everybody who had it chosen.
    QString id;
    QString displayName;
    QString description;
    // A themed icon name, or blank.
    QString iconName;
    PluginFamily family = PluginFamily::Translation;
    // What this backend can do; see ProviderCapability.
    ProviderCapabilities capabilities;
    QVector<ProviderOptionSpec> options;
};

// The half of a plugin that is the same whatever it provides. Not an
// interface in the Q_DECLARE_INTERFACE sense: nothing ever casts to this,
// because a host looking for a TTS backend has no use for something that is
// merely a plugin.
class IProviderPlugin
{
public:
    virtual ~IProviderPlugin() = default;

    // What this backend can be configured with, and where each value lives.
    // The only thing a host has to build the plugin to find out - everything
    // else about it is in the metadata beside it on disk.
    virtual QVector<ProviderOptionSpec> options() const = 0;
};

#endif // IPROVIDERPLUGIN_H
