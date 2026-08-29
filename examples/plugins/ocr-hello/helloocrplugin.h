/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef HELLOOCRPLUGIN_H
#define HELLOOCRPLUGIN_H

#include "plugininterfaces/iocrplugin.h"

#include <QObject>

// Same three requirements as any other family, with the recognition IID. See
// ../translation-hello/hellotranslationplugin.h for what each of them is for.
class HelloOcrPlugin : public QObject
    , public IOcrPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID CROW_OCR_PLUGIN_IID FILE "hello.json")
    Q_INTERFACES(IOcrPlugin)

public:
    QVector<ProviderOptionSpec> options() const override;
    AOcrProvider *createProvider(QObject *parent) override;
    QStringList availableLanguages(const QString &dataPath) const override;
};

#endif // HELLOOCRPLUGIN_H
