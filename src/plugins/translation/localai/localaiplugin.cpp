/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "localaiplugin.h"

#include "language.h"
#include "localaitranslationprovider.h"
#include "llm/knownendpoints.h"

#include <QLocale>

QVector<ProviderOptionSpec> LocalAiTranslationPlugin::options() const
{
    // The one backend whose configuration is a tree rather than a list: which
    // endpoint is active, and then a URL, model, key and timeout for each of
    // the five kinds of endpoint it knows. No single settings key names any of
    // these, so none of them declares one - the host resolves the active
    // endpoint and fills them in, and a settings dialog draws the page by
    // hand because no schema describes a model list fetched over the network.
    //
    // They are declared anyway, with their defaults, so that the contract
    // suite and anything else driving this provider directly can see what it
    // takes.
    return {
        makeDerivedOptionSpec(QStringLiteral("url"), QMetaType::QString, KnownEndpoints::defaultUrl(QStringLiteral("ollama"))),
        makeDerivedOptionSpec(QStringLiteral("model"), QMetaType::QString, KnownEndpoints::defaultModel(QStringLiteral("ollama"))),
        makeDerivedOptionSpec(QStringLiteral("prompt"), QMetaType::QString, LocalAiTranslationProvider::minimalPrompt()),
        makeDerivedOptionSpec(QStringLiteral("disable_thinking"), QMetaType::Bool, false),
        makeDerivedOptionSpec(QStringLiteral("timeout"), QMetaType::Int, KnownEndpoints::defaultTimeoutSeconds()),
        makeDerivedOptionSpec(QStringLiteral("api_key"), QMetaType::QString, QString()),
        makeDerivedOptionSpec(QStringLiteral("is_anthropic"), QMetaType::Bool, false),
        // The application's primary language. Used when nothing detected and
        // no detection model is configured; it is a fact about the
        // application's settings, not about this endpoint, which is why it
        // arrives as an option instead of being read here.
        makeDerivedOptionSpec(QStringLiteral("fallback_language"), QMetaType::QString, Language(QLocale::system()).toCode()),
        makeDerivedOptionSpec(QStringLiteral("detect_url"), QMetaType::QString, KnownEndpoints::defaultUrl(QStringLiteral("ollama"))),
        makeDerivedOptionSpec(QStringLiteral("detect_model"), QMetaType::QString, QString()),
        makeDerivedOptionSpec(QStringLiteral("detect_api_key"), QMetaType::QString, QString()),
        makeDerivedOptionSpec(QStringLiteral("detect_is_anthropic"), QMetaType::Bool, false),
    };
}

ATranslationProvider *LocalAiTranslationPlugin::createProvider(QObject *parent)
{
    return new LocalAiTranslationProvider(parent);
}
