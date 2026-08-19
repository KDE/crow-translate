/*
 * SPDX-FileCopyrightText: 2026 Oleksandr Mikriukov <ur3ley@gmail.com>
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef OPENAIENDPOINT_H
#define OPENAIENDPOINT_H

#include <QString>

class QByteArray;
class QNetworkRequest;

// Wire-level helpers for OpenAI-compatible and Anthropic-compatible HTTP
// endpoints: turning whatever URL a user pasted into the concrete endpoint to
// call, attaching the right auth headers, and pulling the assistant text back
// out of a response.
//
// Deliberately provider-neutral and free of any translation or OCR concept:
// LocalAiTranslationProvider and LlmOcr are independently configured features
// that happen to speak the same two API shapes, so neither should have to
// include the other to talk to its own endpoint.
namespace OpenAiEndpoint
{

// Endpoint to POST a chat completion / message to.
QString completionsUrl(const QString &baseUrl, bool isAnthropic);

// Endpoint to GET the model list from.
QString modelsUrl(const QString &baseUrl);

// The server root a pasted URL refers to, with any trailing slashes and any
// recognized endpoint path stripped. Callers that need a non-OpenAI path on
// the same host - Ollama's "/api/tags", LM Studio's "/api/v0/models" - build
// it from here so they honor the same paste-anything normalization.
QString serverRoot(const QString &baseUrl);

// Bearer for OpenAI-compatible, x-api-key + anthropic-version for Anthropic.
// A blank key leaves the request untouched (local servers need no auth).
void setAuthHeaders(QNetworkRequest &request, bool isAnthropic, const QString &apiKey);

// Everything a caller needs to explain a completions/messages response,
// including the ones that carry no text. An empty `content` on its own is not
// diagnosable - a thinking model that spends its whole budget reasoning, a
// prompt that left no room to answer, and a server-side error all look
// identical - so the fields that tell them apart come back too.
struct Completion {
    QString content;
    // "stop", "length", ... - "length" with empty content means the model ran
    // out of budget before it said anything.
    QString finishReason;
    // Reasoning models put their working here and it does not count as
    // output; a non-empty value with empty content is a budget that went on
    // thinking.
    QString reasoning;
    // From the response's own "usage" block; -1 when the server omits it.
    int promptTokens = -1;
    int completionTokens = -1;
    int totalTokens = -1;
    // The provider's own message from an "error" object, which is far more
    // useful than the transport-level string Qt would give us.
    QString errorMessage;

    bool hasText() const
    {
        return !content.isEmpty();
    }
    bool ranOutOfBudget() const
    {
        return finishReason == QLatin1String("length");
    }
    bool spentBudgetReasoning() const
    {
        return !reasoning.isEmpty();
    }
};

// Parse a completions/messages response body.
Completion parseCompletion(const QByteArray &data, bool isAnthropic);

} // namespace OpenAiEndpoint

#endif // OPENAIENDPOINT_H
