/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TESSERACTPLUGIN_H
#define TESSERACTPLUGIN_H

#include "plugininterfaces/iocrplugin.h"

#include <QObject>

class TesseractOcrPlugin : public QObject
    , public IOcrPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID CROW_OCR_PLUGIN_IID FILE "tesseract.json")
    Q_INTERFACES(IOcrPlugin)

public:
    QVector<ProviderOptionSpec> options() const override;
    AOcrProvider *createProvider(QObject *parent) override;
    QStringList availableLanguages(const QString &dataPath) const override;
};

#endif // TESSERACTPLUGIN_H
