/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TRANSLATIONLOGIC_H
#define TRANSLATIONLOGIC_H

#include "language.h"

#include <QLocale>

// The auto (target / source) language rule, shared by every translation
// backend so none of them can drift. This has regressed repeatedly; the
// canonical behaviour lives here and is pinned by tests/test_translationlogic.cpp.
namespace TranslationLogic
{

// True when the two languages are the same for translation purposes: identical
// locale, or the same base language ignoring script/territory (en == en_US).
// A non-locale language only matches itself exactly.
bool sameLanguage(const Language &a, const Language &b);

// The destination a backend should pick when the user asked for "auto".
// source is the (already known) source language; primary/secondary come from
// the settings; fallback (normally the system locale) is used when a primary
// or secondary isn't configured.
//   - source != primary   -> primary
//   - source == primary   -> secondary
//   - source equals both  -> fallback
Language preferredDestination(const Language &source,
                              const Language &primary,
                              const Language &secondary,
                              const Language &fallback);

} // namespace TranslationLogic

#endif // TRANSLATIONLOGIC_H