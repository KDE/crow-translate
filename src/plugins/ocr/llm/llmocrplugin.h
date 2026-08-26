/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LLMOCRPLUGIN_H
#define LLMOCRPLUGIN_H

#include "plugininterfaces/iocrplugin.h"

#include <QObject>

class LlmOcrPlugin : public QObject
    , public IOcrPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID CROW_OCR_PLUGIN_IID FILE "llm.json")
    Q_INTERFACES(IOcrPlugin)

public:
    QVector<ProviderOptionSpec> options() const override;
    AOcrProvider *createProvider(QObject *parent) override;
};

#endif // LLMOCRPLUGIN_H
