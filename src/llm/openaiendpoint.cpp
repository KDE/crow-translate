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

QString extractContent(const QByteArray &data, bool isAnthropic)
{
    const QJsonObject obj = QJsonDocument::fromJson(data).object();
    if (isAnthropic) {
        const QJsonArray blocks = obj.value(QStringLiteral("content")).toArray();
        for (const QJsonValue &block : blocks) {
            const QJsonObject blockObj = block.toObject();
            if (blockObj.value(QStringLiteral("type")).toString() == QLatin1String("text")) {
                return blockObj.value(QStringLiteral("text")).toString();
            }
        }
        return QString();
    }
    const QJsonArray choices = obj.value(QStringLiteral("choices")).toArray();
    if (!choices.isEmpty()) {
        return choices.first().toObject().value(QStringLiteral("message")).toObject().value(QStringLiteral("content")).toString();
    }
    // Fallback for Ollama native shape, just in case.
    return obj.value(QStringLiteral("message")).toObject().value(QStringLiteral("content")).toString();
}

} // namespace OpenAiEndpoint
