/*
 * SPDX-FileCopyrightText: 2026 Oleksandr Mikriukov <ur3ley@gmail.com>
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "openaiendpoint.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QStringList>
#include <QUrl>

namespace OpenAiEndpoint
{

QString completionsUrl(const QString &baseUrl, bool isAnthropic)
{
    // Users paste either a base URL ("https://api.example.com/v1", as handed
    // to official SDKs) or the full endpoint they got from docs
    // (".../v1/chat/completions"). Always appending the suffix produced a
    // doubled path for the latter. Detect which one was entered and normalize:
    // strip trailing slashes, then append the kind-specific suffix only when
    // the URL does not already end in it. Only suffixes this code actually
    // builds a matching request body for are recognized here - notably NOT
    // Ollama's native "/api/chat" (its request/response shape differs from
    // the OpenAI-compatible one callers always send), so a URL pointed there
    // falls through to getting a suffix appended instead of being silently
    // treated as complete. Ollama also serves an OpenAI-compatible
    // "/v1/chat/completions" endpoint, which is what this code targets by
    // default.
    QString base = baseUrl.trimmed();
    while (base.endsWith(QLatin1Char('/'))) {
        base.chop(1);
    }

    static const QStringList kCompletionsSuffixes = {
        QStringLiteral("/v1/chat/completions"),
        QStringLiteral("/v1/messages"), // Anthropic
    };

    for (const QString &suffix : kCompletionsSuffixes) {
        if (base.endsWith(suffix, Qt::CaseInsensitive)) {
            // Already a full completions endpoint: use as typed. An
            // Anthropic-style "/v1/messages" URL in an OpenAI-kind provider
            // (or vice versa) is still honored as-is - the user pointed at a
            // concrete endpoint and knows better than the suffix defaults.
            return base;
        }
    }
    // A base that already carries a path (e.g. z.ai's
    // ".../api/coding/paas/v4") is the complete SDK base URL: append only the
    // kind suffix, as the official SDK would. Only a bare host (no path,
    // crow's default "...:11434") needs the conventional "/v1" segment
    // prepended.
    const QUrl url(base);
    const QString path = url.path();
    const bool bareHost = path.isEmpty() || path == QLatin1String("/");
    if (isAnthropic) {
        return base + QStringLiteral("/v1/messages");
    }
    return bareHost ? base + QStringLiteral("/v1/chat/completions")
                    : base + QStringLiteral("/chat/completions");
}

QString serverRoot(const QString &baseUrl)
{
    // If the user pasted a full endpoint URL (any of the completions shapes),
    // derive its base by stripping the endpoint path so a probe does not end
    // up at ".../chat/completions/v1/models".
    QString base = baseUrl.trimmed();
    while (base.endsWith(QLatin1Char('/'))) {
        base.chop(1);
    }

    static const QStringList kEndpointSuffixes = {
        QStringLiteral("/v1/chat/completions"),
        QStringLiteral("/v1/messages"),
        QStringLiteral("/v1/models"),
    };

    for (const QString &suffix : kEndpointSuffixes) {
        if (base.endsWith(suffix, Qt::CaseInsensitive)) {
            base.chop(suffix.size());
            break;
        }
    }
    while (base.endsWith(QLatin1Char('/'))) {
        base.chop(1);
    }
    return base;
}

QString modelsUrl(const QString &baseUrl)
{
    const QString base = serverRoot(baseUrl);
    // A base that already carries a path (e.g. z.ai's
    // ".../api/coding/paas/v4") is the complete SDK base: the probe lives at
    // "<base>/models". A bare host (no path) needs the conventional "/v1"
    // segment: "<host>/v1/models".
    const QUrl url(base);
    const QString path = url.path();
    if (path.isEmpty() || path == QLatin1String("/")) {
        return base + QStringLiteral("/v1/models");
    }
    return base + QStringLiteral("/models");
}

void setAuthHeaders(QNetworkRequest &request, bool isAnthropic, const QString &apiKey)
{
    if (apiKey.isEmpty()) {
        return;
    }
    if (isAnthropic) {
        request.setRawHeader("x-api-key", apiKey.toUtf8());
        request.setRawHeader("anthropic-version", "2023-06-01");
    } else {
        request.setRawHeader("Authorization", "Bearer " + apiKey.toUtf8());
    }
}

Completion parseCompletion(const QByteArray &data, bool isAnthropic)
{
    Completion completion;
    const QJsonObject obj = QJsonDocument::fromJson(data).object();

    // Both shapes report failures as an "error" object, and its message says
    // far more than any transport-level error string.
    const QJsonValue errorValue = obj.value(QStringLiteral("error"));
    if (errorValue.isObject()) {
        completion.errorMessage = errorValue.toObject().value(QStringLiteral("message")).toString();
    } else if (errorValue.isString()) {
        // Ollama's native shape puts a bare string here.
        completion.errorMessage = errorValue.toString();
    }

    const QJsonObject usage = obj.value(QStringLiteral("usage")).toObject();
    if (!usage.isEmpty()) {
        // Anthropic names them differently from everyone else.
        completion.promptTokens = usage.value(isAnthropic ? QStringLiteral("input_tokens") : QStringLiteral("prompt_tokens")).toInt(-1);
        completion.completionTokens = usage.value(isAnthropic ? QStringLiteral("output_tokens") : QStringLiteral("completion_tokens")).toInt(-1);
        completion.totalTokens = usage.value(QStringLiteral("total_tokens")).toInt(-1);
        if (completion.totalTokens < 0 && completion.promptTokens >= 0 && completion.completionTokens >= 0) {
            completion.totalTokens = completion.promptTokens + completion.completionTokens;
        }
    }

    if (isAnthropic) {
        completion.finishReason = obj.value(QStringLiteral("stop_reason")).toString();
        if (completion.finishReason == QLatin1String("max_tokens")) {
            completion.finishReason = QStringLiteral("length");
        }
        const QJsonArray blocks = obj.value(QStringLiteral("content")).toArray();
        for (const QJsonValue &block : blocks) {
            const QJsonObject blockObj = block.toObject();
            const QString type = blockObj.value(QStringLiteral("type")).toString();
            if (type == QLatin1String("text") && completion.content.isEmpty()) {
                completion.content = blockObj.value(QStringLiteral("text")).toString();
            } else if (type == QLatin1String("thinking")) {
                completion.reasoning += blockObj.value(QStringLiteral("thinking")).toString();
            }
        }
        return completion;
    }

    const QJsonArray choices = obj.value(QStringLiteral("choices")).toArray();
    if (!choices.isEmpty()) {
        const QJsonObject choice = choices.first().toObject();
        completion.finishReason = choice.value(QStringLiteral("finish_reason")).toString();
        const QJsonObject message = choice.value(QStringLiteral("message")).toObject();
        completion.content = message.value(QStringLiteral("content")).toString();
        // Ollama exposes a reasoning model's working as "reasoning"; some
        // OpenAI-compatible servers use "reasoning_content".
        completion.reasoning = message.value(QStringLiteral("reasoning")).toString();
        if (completion.reasoning.isEmpty()) {
            completion.reasoning = message.value(QStringLiteral("reasoning_content")).toString();
        }
        return completion;
    }

    // Fallback for Ollama native shape, just in case.
    const QJsonObject message = obj.value(QStringLiteral("message")).toObject();
    completion.content = message.value(QStringLiteral("content")).toString();
    completion.reasoning = message.value(QStringLiteral("thinking")).toString();
    return completion;
}

} // namespace OpenAiEndpoint
