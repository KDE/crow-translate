/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "translationlogic.h"

namespace TranslationLogic
{

bool sameLanguage(const Language &a, const Language &b)
{
    if (a == b) {
        return true;
    }
    if (a.hasQLocaleEquivalent() && b.hasQLocaleEquivalent()) {
        return a.toQLocale().language() == b.toQLocale().language();
    }
    return false;
}

Language preferredDestination(const Language &source,
                              const Language &primary,
                              const Language &secondary,
                              const Language &fallback)
{
    const Language primaryLang = (primary == Language::autoLanguage()) ? fallback : primary;
    if (!sameLanguage(primaryLang, source)) {
        return primaryLang;
    }
    const Language secondaryLang = (secondary == Language::autoLanguage()) ? fallback : secondary;
    if (!sameLanguage(secondaryLang, source)) {
        return secondaryLang;
    }
    return fallback;
}

} // namespace TranslationLogic