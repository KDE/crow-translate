/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "languageresolution.h"

#include "translationlogic.h"

#include <functional>

LanguageResolution::LanguageResolution(QObject *parent)
    : QObject(parent)
    , m_primary(QLocale::system())
    , m_secondary(QLocale::system())
{
}

// Every setter routes through here so "did the answer move" is decided by
// comparing the outputs, not by guessing which inputs matter to which output.
void LanguageResolution::emitIfChanged(const std::function<void()> &apply)
{
    const Language wasSource = effectiveSource();
    const Language wasDestination = effectiveDestination();
    const std::optional<Language> wasTranslatedSource = m_translatedSource;
    const std::optional<Language> wasTranslatedDestination = m_translatedDestination;

    apply();

    if (effectiveSource() != wasSource || effectiveDestination() != wasDestination
        || m_translatedSource != wasTranslatedSource || m_translatedDestination != wasTranslatedDestination) {
        emit changed();
    }
}

void LanguageResolution::setSelected(const Language &source, const Language &destination)
{
    emitIfChanged([this, &source, &destination]() {
        m_selectedSource = source;
        m_selectedDestination = destination;
    });
}

void LanguageResolution::setPreference(const Language &primary, const Language &secondary, const Language &fallback)
{
    emitIfChanged([this, &primary, &secondary, &fallback]() {
        m_primary = primary;
        m_secondary = secondary;
        m_fallback = fallback;
    });
}

void LanguageResolution::setDetectedSource(const Language &source)
{
    emitIfChanged([this, &source]() {
        m_detectedSource = source == Language::autoLanguage() ? std::nullopt : std::optional<Language>(source);
    });
}

void LanguageResolution::setTranslated(const Language &source, const Language &destination)
{
    emitIfChanged([this, &source, &destination]() {
        m_translatedSource = source == Language::autoLanguage() ? std::nullopt : std::optional<Language>(source);
        m_translatedDestination = destination == Language::autoLanguage() ? std::nullopt : std::optional<Language>(destination);
    });
}

void LanguageResolution::forgetTranslation()
{
    emitIfChanged([this]() {
        m_translatedSource.reset();
        m_translatedDestination.reset();
    });
}

std::optional<Language> LanguageResolution::translatedSource() const
{
    return m_translatedSource;
}

std::optional<Language> LanguageResolution::translatedDestination() const
{
    return m_translatedDestination;
}

Language LanguageResolution::predictedSource() const
{
    if (m_selectedSource != Language::autoLanguage()) {
        return m_selectedSource;
    }
    // Auto: what detection found beats guessing, and the fallback is only a
    // guess - which is why it must never outrank a fact.
    return m_detectedSource.value_or(m_fallback);
}

Language LanguageResolution::predictedDestination() const
{
    if (m_selectedDestination != Language::autoLanguage()) {
        return m_selectedDestination;
    }
    return TranslationLogic::preferredDestination(predictedSource(), m_primary, m_secondary, m_fallback);
}

Language LanguageResolution::effectiveSource() const
{
    if (m_selectedSource != Language::autoLanguage()) {
        return m_selectedSource;
    }
    return m_translatedSource.value_or(predictedSource());
}

Language LanguageResolution::effectiveDestination() const
{
    if (m_selectedDestination != Language::autoLanguage()) {
        return m_selectedDestination;
    }
    return m_translatedDestination.value_or(predictedDestination());
}
