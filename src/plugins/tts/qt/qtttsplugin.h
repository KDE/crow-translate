/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef QTTTSPLUGIN_H
#define QTTTSPLUGIN_H

#include "plugininterfaces/ittsplugin.h"

#include <QObject>

class QtTtsPlugin : public QObject
    , public ITtsPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID CROW_TTS_PLUGIN_IID FILE "qt.json")
    Q_INTERFACES(ITtsPlugin)

public:
    QVector<ProviderOptionSpec> options() const override;
    ATTSProvider *createProvider(QObject *parent) override;
};

#endif // QTTTSPLUGIN_H
