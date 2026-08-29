/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef FOREIGNPLUGIN_H
#define FOREIGNPLUGIN_H

#include "plugininterfaces/itranslationplugin.h"

#include <QObject>

// A backend that this tree does not ship, built the way one from outside it
// would be: a MODULE library over Crow::Core, no crow_add_plugin(), and no
// participation in CROW_STATIC_PLUGINS. It exists so that "a module in the
// search path is found" is a thing the suite asserts rather than a thing the
// dynamic build happens to demonstrate.
//
// The provider it builds is Crow's own Copy backend, because what is under
// test here is discovery, and a second hand-written provider would only be a
// second thing to keep in step.
class ForeignTranslationPlugin : public QObject
    , public ITranslationPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID CROW_TRANSLATION_PLUGIN_IID FILE "foreign.json")
    Q_INTERFACES(ITranslationPlugin)

public:
    QVector<ProviderOptionSpec> options() const override;
    ATranslationProvider *createProvider(QObject *parent) override;
};

#endif // FOREIGNPLUGIN_H
