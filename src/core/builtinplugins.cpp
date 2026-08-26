/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/builtinplugins.h"

#include "translator/copytranslationprovider.h"
#include "tts/noopttsprovider.h"

CopyTranslationPlugin::CopyTranslationPlugin(QObject *parent)
    : QObject(parent)
{
}

QVector<ProviderOptionSpec> CopyTranslationPlugin::options() const
{
    // Nothing to configure: it hands the text back.
    return {};
}

ATranslationProvider *CopyTranslationPlugin::createProvider(QObject *parent)
{
    return new CopyTranslationProvider(parent);
}

NoopTtsPlugin::NoopTtsPlugin(QObject *parent)
    : QObject(parent)
{
}

QVector<ProviderOptionSpec> NoopTtsPlugin::options() const
{
    return {};
}

ATTSProvider *NoopTtsPlugin::createProvider(QObject *parent)
{
    return new NoopTTSProvider(parent);
}
