/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef PIPERTTSPLUGIN_H
#define PIPERTTSPLUGIN_H

#include "plugininterfaces/ittsplugin.h"

#include <QObject>

class PiperTtsPlugin : public QObject
    , public ITtsPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID CROW_TTS_PLUGIN_IID FILE "piper.json")
    Q_INTERFACES(ITtsPlugin)

public:
    QVector<ProviderOptionSpec> options() const override;
    ATTSProvider *createProvider(QObject *parent) override;
};

#endif // PIPERTTSPLUGIN_H
