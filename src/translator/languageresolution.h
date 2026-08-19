/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LANGUAGERESOLUTION_H
#define LANGUAGERESOLUTION_H

#include "language.h"

#include <QObject>

#include <functional>
#include <optional>

// The one place that knows which languages a translation is actually between.
//
// What the user selected, what detection found and what the provider used are
// three different facts, and every consumer used to combine them itself:
// speech, the voice combo boxes, the auto button's label and the pop-up each
// kept their own answer, updated by hand from whichever handler happened to
// notice. Anything set on only one code path went stale in silence - which is
// the bug behind an English voice reading a Russian translation, a voice list
// one translation behind, and an auto button that could only ever say
// "Auto (en)". Adding a backend meant finding all of those sites again.
//
// Inputs go in, the outputs below come out, and changed() says when they
// moved. Consumers subscribe; none of them re-derives.
class LanguageResolution : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(LanguageResolution)

public:
    explicit LanguageResolution(QObject *parent = nullptr);

    // --- inputs -----------------------------------------------------------

    // The language buttons' checked languages. Language::autoLanguage() means
    // the user asked for auto and the answer has to be resolved.
    void setSelected(const Language &source, const Language &destination);

    // primary/secondary/fallback for the auto rule. Passed in rather than read
    // from AppSettings so this class is testable without touching settings.
    void setPreference(const Language &primary, const Language &secondary, const Language &fallback);

    // What language detection reported for the current source text.
    void setDetectedSource(const Language &source);

    // What the provider actually translated between. autoLanguage() for either
    // means "it has not said yet", which is not the same as a guess.
    void setTranslated(const Language &source, const Language &destination);

    // The source text changed, so what the last translation used no longer
    // describes what is on screen.
    void forgetTranslation();

    // --- outputs ----------------------------------------------------------

    // What a translation actually happened between, when one has. nullopt is
    // the honest answer before that, and callers have to handle it rather than
    // render a guess as though it were fact.
    std::optional<Language> translatedSource() const;
    std::optional<Language> translatedDestination() const;

    // What the next translation would be between, given the selection, the
    // detection so far and the preference rule. Always defined.
    Language predictedSource() const;
    Language predictedDestination() const;

    // What to show and to speak: the real answer where there is one, the
    // prediction otherwise. This is what the UI wants, and building it from
    // the parts is exactly the mistake this class exists to remove.
    Language effectiveSource() const;
    Language effectiveDestination() const;

signals:
    // Any of the outputs above moved.
    void changed();

private:
    void emitIfChanged(const std::function<void()> &apply);

    Language m_selectedSource = Language::autoLanguage();
    Language m_selectedDestination = Language::autoLanguage();
    Language m_primary;
    Language m_secondary;
    Language m_fallback = Language(QLocale::system());
    std::optional<Language> m_detectedSource;
    std::optional<Language> m_translatedSource;
    std::optional<Language> m_translatedDestination;
};

#endif // LANGUAGERESOLUTION_H
