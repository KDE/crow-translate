/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef KNOWNENDPOINTS_H
#define KNOWNENDPOINTS_H

#include "crow_core_export.h"

#include <QString>
#include <QStringList>

// The local and remote model servers crow knows how to talk to out of the box.
//
// A table, not a policy: which port Ollama listens on and whether Anthropic
// speaks its own API shape are facts about those servers. It sat in
// AppSettings, which made it look like application configuration, and put it
// out of reach of a backend that is a loadable module.
//
// Three unrelated features consult it - the LocalAI translation backend, the
// vision-model OCR engine, and the settings pages for both - so it belongs
// beside OpenAiEndpoint, which is already the provider-neutral half of
// talking to one of these.
namespace KnownEndpoints
{

CROW_CORE_EXPORT QStringList ids();
CROW_CORE_EXPORT QString displayName(const QString &id);
CROW_CORE_EXPORT QString defaultUrl(const QString &id);
CROW_CORE_EXPORT QString defaultModel(const QString &id);

// Whether this one speaks Anthropic's Messages API shape (/v1/messages,
// x-api-key) rather than the OpenAI-compatible one every other id uses.
CROW_CORE_EXPORT bool isAnthropic(const QString &id);

// How long to wait for one of these to answer. Generous on purpose: a local
// model on a cold cache can take minutes to produce its first token.
CROW_CORE_EXPORT int defaultTimeoutSeconds();

} // namespace KnownEndpoints

#endif // KNOWNENDPOINTS_H
