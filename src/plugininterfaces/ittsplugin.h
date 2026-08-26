/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef ITTSPLUGIN_H
#define ITTSPLUGIN_H

#include "plugininterfaces/iproviderplugin.h"
#include "tts/attsprovider.h"

#include <QtPlugin>

#define CROW_TTS_PLUGIN_IID "org.kde.CrowTranslate.TtsProvider/1.0"

// A speech backend. See ITranslationPlugin for why this is as thin as it is.
class ITtsPlugin : public IProviderPlugin
{
public:
    ~ITtsPlugin() override = default;

    virtual ATTSProvider *createProvider(QObject *parent) = 0;
};

Q_DECLARE_INTERFACE(ITtsPlugin, CROW_TTS_PLUGIN_IID)

#endif // ITTSPLUGIN_H
