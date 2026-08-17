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

// Assistant text out of a completions/messages response body.
QString extractContent(const QByteArray &data, bool isAnthropic);

} // namespace OpenAiEndpoint

#endif // OPENAIENDPOINT_H
