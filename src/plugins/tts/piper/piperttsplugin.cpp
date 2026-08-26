/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "piperttsplugin.h"

#include "piperttsprovider.h"

QVector<ProviderOptionSpec> PiperTtsPlugin::options() const
{
    return {
        makeOptionSpec(QStringLiteral("voices_path"),
                       QStringLiteral("TTS/PiperVoicesPath"),
                       QMetaType::QByteArray,
                       QByteArray(),
                       tr("Voice models directory"),
                       tr("Searched before the standard locations for Piper voice models.")),
        // Which speaker within the current voice. Per-voice rather than
        // per-installation, so there is no settings key that would mean the
        // same thing after the voice changes.
        makeDerivedOptionSpec(QStringLiteral("speaker"), QMetaType::QString, QStringLiteral("default")),
        // Not a setting at all: the host raises it to tell the backend the
        // voice models may have moved.
        makeDerivedOptionSpec(QStringLiteral("reinitializeModels"), QMetaType::Bool, true),
    };
}

ATTSProvider *PiperTtsPlugin::createProvider(QObject *parent)
{
    return new PiperTTSProvider(parent);
}
