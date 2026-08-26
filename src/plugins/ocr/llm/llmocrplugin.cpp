/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "llmocrplugin.h"

#include "llmocr.h"
#include "llm/knownendpoints.h"

QVector<ProviderOptionSpec> LlmOcrPlugin::options() const
{
    // Every value here lives under its own endpoint id in the settings file -
    // the endpoint that transcribes your screenshots is not the one that
    // translates your text - so none of them is a single fixed key and the
    // host resolves the active endpoint before filling them in.
    return {
        makeDerivedOptionSpec(QStringLiteral("url"), QMetaType::QString, KnownEndpoints::defaultUrl(QStringLiteral("ollama"))),
        makeDerivedOptionSpec(QStringLiteral("is_anthropic"), QMetaType::Bool, false),
        makeDerivedOptionSpec(QStringLiteral("api_key"), QMetaType::QString, QString()),
        makeDerivedOptionSpec(QStringLiteral("model"), QMetaType::QString, QString()),
        makeDerivedOptionSpec(QStringLiteral("timeout"), QMetaType::Int, KnownEndpoints::defaultTimeoutSeconds()),
        makeDerivedOptionSpec(QStringLiteral("prompt"), QMetaType::QString, LlmOcr::defaultPrompt()),
        makeDerivedOptionSpec(QStringLiteral("disable_thinking"), QMetaType::Bool, false),
    };
}

AOcrProvider *LlmOcrPlugin::createProvider(QObject *parent)
{
    return new LlmOcr(parent);
}
