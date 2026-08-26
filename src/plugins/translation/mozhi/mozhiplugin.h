/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef MOZHIPLUGIN_H
#define MOZHIPLUGIN_H

#include "plugininterfaces/itranslationplugin.h"

#include <QObject>

class MozhiTranslationPlugin : public QObject
    , public ITranslationPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID CROW_TRANSLATION_PLUGIN_IID FILE "mozhi.json")
    Q_INTERFACES(ITranslationPlugin)

public:
    QVector<ProviderOptionSpec> options() const override;
    ATranslationProvider *createProvider(QObject *parent) override;
};

#endif // MOZHIPLUGIN_H
