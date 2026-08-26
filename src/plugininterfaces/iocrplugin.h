/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef IOCRPLUGIN_H
#define IOCRPLUGIN_H

#include "ocr/aocrprovider.h"
#include "plugininterfaces/iproviderplugin.h"

#include <QStringList>
#include <QtPlugin>

#define CROW_OCR_PLUGIN_IID "org.kde.CrowTranslate.OcrProvider/1.0"

// A recognition engine.
//
// Carries one thing the other two families do not need. The settings dialog
// offers a list of languages to recognize in, and that list comes from
// whatever the engine finds in its data directory - Tesseract's trained
// data, today. Asking the engine is the last place the application reached
// for a concrete class by name.
class IOcrPlugin : public IProviderPlugin
{
public:
    ~IOcrPlugin() override = default;

    virtual AOcrProvider *createProvider(QObject *parent) = 0;

    // Languages this engine can recognize with the data at the given path.
    // Empty for an engine with no such notion: a vision model is not
    // configured by language, and has nothing to answer with.
    virtual QStringList availableLanguages(const QString &dataPath) const
    {
        Q_UNUSED(dataPath)
        return {};
    }
};

Q_DECLARE_INTERFACE(IOcrPlugin, CROW_OCR_PLUGIN_IID)

#endif // IOCRPLUGIN_H
