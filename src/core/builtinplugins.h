/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef BUILTINPLUGINS_H
#define BUILTINPLUGINS_H

#include "plugininterfaces/itranslationplugin.h"
#include "plugininterfaces/ittsplugin.h"

#include <QObject>

// The two backends that cannot be loadable.
//
// They are what the host falls back to when a module throws, or when the
// plugin directory is not there at all - a machine where the crow-translate
// package is installed and the crow-translate-plugins one is not. A fallback
// that can itself be missing is not a fallback, so these are compiled into
// the core.
//
// They are QObjects with Q_INTERFACES for exactly one reason: so that
// PluginManager reaches a built-in and a loaded module through the same
// qobject_cast, instead of keeping a second code path for the two backends
// that must never fail. Their identity is written by PluginManager, which is
// the one place that already has to know they exist; they carry no metadata
// file because there is no file.

class CopyTranslationPlugin : public QObject
    , public ITranslationPlugin
{
    Q_OBJECT
    Q_INTERFACES(ITranslationPlugin)

public:
    explicit CopyTranslationPlugin(QObject *parent = nullptr);

    QVector<ProviderOptionSpec> options() const override;
    ATranslationProvider *createProvider(QObject *parent) override;
};

class NoopTtsPlugin : public QObject
    , public ITtsPlugin
{
    Q_OBJECT
    Q_INTERFACES(ITtsPlugin)

public:
    explicit NoopTtsPlugin(QObject *parent = nullptr);

    QVector<ProviderOptionSpec> options() const override;
    ATTSProvider *createProvider(QObject *parent) override;
};

#endif // BUILTINPLUGINS_H
