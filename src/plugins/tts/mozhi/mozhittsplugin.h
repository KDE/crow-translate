/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef MOZHITTSPLUGIN_H
#define MOZHITTSPLUGIN_H

#include "plugininterfaces/ittsplugin.h"

#include <QObject>

class MozhiTtsPlugin : public QObject
    , public ITtsPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID CROW_TTS_PLUGIN_IID FILE "mozhi.json")
    Q_INTERFACES(ITtsPlugin)

public:
    QVector<ProviderOptionSpec> options() const override;
    ATTSProvider *createProvider(QObject *parent) override;
};

#endif // MOZHITTSPLUGIN_H
