/*
 * SPDX-FileCopyrightText: 2026 Oleksandr Mikriukov <ur3ley@gmail.com>
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "llm/knownendpoints.h"

#include <QCoreApplication>

QStringList KnownEndpoints::ids()
{
    return {QStringLiteral("ollama"), QStringLiteral("fastflowlm"), QStringLiteral("lmstudio"),
            QStringLiteral("openai_custom"), QStringLiteral("anthropic")};
}

QString KnownEndpoints::displayName(const QString &id)
{
    if (id == QLatin1String("ollama")) {
        return QStringLiteral("Ollama");
    }
    if (id == QLatin1String("fastflowlm")) {
        return QStringLiteral("FastFlowLM");
    }
    if (id == QLatin1String("lmstudio")) {
        return QStringLiteral("LM Studio");
    }
    if (id == QLatin1String("openai_custom")) {
        // Deliberately still translated in AppSettings' context: the string is
        // unchanged and every translation of it is filed under that context.
        return QCoreApplication::translate("AppSettings", "OpenAI-compatible (custom)");
    }
    if (id == QLatin1String("anthropic")) {
        return QStringLiteral("Anthropic");
    }
    return id;
}

QString KnownEndpoints::defaultUrl(const QString &id)
{
    if (id == QLatin1String("fastflowlm")) {
        return QStringLiteral("http://localhost:8082");
    }
    if (id == QLatin1String("lmstudio")) {
        return QStringLiteral("http://localhost:1234");
    }
    if (id == QLatin1String("openai_custom")) {
        return QString(); // no sensible default - user's own/cloud endpoint
    }
    if (id == QLatin1String("anthropic")) {
        return QStringLiteral("https://api.anthropic.com");
    }
    return QStringLiteral("http://localhost:11434"); // ollama
}

QString KnownEndpoints::defaultModel(const QString &id)
{
    if (id == QLatin1String("ollama")) {
        return QStringLiteral("translategemma_12b_128k:latest");
    }
    return QString();
}

bool KnownEndpoints::isAnthropic(const QString &id)
{
    return id == QLatin1String("anthropic");
}

int KnownEndpoints::defaultTimeoutSeconds()
{
    return 300;
}
