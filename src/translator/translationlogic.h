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

// "Force source language detection" and "Force translation language
// detection" (both on as installed): a global action switches the source, the
// destination or both to Auto before it runs, whatever is chosen on the
// buttons. Which side for which action is decided here and nowhere else - it
// used to be written out in each action, and the copies drifted.
enum class ForcedFlow {
    TranslateSelection, // translate, speak-translated and copy-translated selection
    SpeakSelection, // read the selection out: nothing is translated
    Recognition, // every way a recognition starts: screen area, delayed, an image
};

struct ForcedAuto {
    bool source = false;
    bool destination = false;

    bool operator==(const ForcedAuto &other) const
    {
        return source == other.source && destination == other.destination;
    }
};

// forceSource/forceDestination are the two settings. engineDetects is whether
// the translation backend in use can find out what language a text is in:
// one that cannot (DeepL and Reverso through Mozhi, the Copy backend) has no
// use for an automatic source and fails on it, so the source the user chose
// is kept - as it was until 4.0.0 dropped the check.
ForcedAuto forcedAuto(ForcedFlow flow, bool forceSource, bool forceDestination, bool engineDetects);

} // namespace TranslationLogic

#endif // TRANSLATIONLOGIC_H