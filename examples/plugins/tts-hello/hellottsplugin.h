/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef HELLOTTSPLUGIN_H
#define HELLOTTSPLUGIN_H

#include "plugininterfaces/ittsplugin.h"

#include <QObject>

// Same three requirements as any other family, with the speech IID. See
// ../translation-hello/hellotranslationplugin.h for what each of them is for.
class HelloTtsPlugin : public QObject
    , public ITtsPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID CROW_TTS_PLUGIN_IID FILE "hello.json")
    Q_INTERFACES(ITtsPlugin)

public:
    QVector<ProviderOptionSpec> options() const override;
    ATTSProvider *createProvider(QObject *parent) override;
};

#endif // HELLOTTSPLUGIN_H
