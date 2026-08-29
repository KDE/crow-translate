/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

// LanguageResolution owns the answer to "which languages is this translation
// between", which used to be re-derived by every consumer. These pin the
// answers the consumers depend on, without a window or a provider in sight.

#include "translator/languageresolution.h"

#include <QSignalSpy>
#include <QTest>

class LanguageResolutionTest : public QObject
{
    Q_OBJECT

private:
    static Language english()
    {
        return Language(QLocale::English);
    }
    static Language russian()
    {
        return Language(QLocale::Russian);
    }
    static Language polish()
    {
        return Language(QLocale::Polish);
    }
    static Language german()
    {
        return Language(QLocale::German);
    }

    static void configure(LanguageResolution &resolution)
    {
        resolution.setPreference(english(), russian(), english());
    }

private slots:
    // An explicit selection is the answer, whatever anything else reports.
    void testExplicitSelectionWins()
    {
        LanguageResolution resolution;
        configure(resolution);
        resolution.setSelected(polish(), german());
        resolution.setDetectedSource(english());
        resolution.setTranslated(english(), russian());

        QCOMPARE(resolution.effectiveSource(), polish());
        QCOMPARE(resolution.effectiveDestination(), german());
        QCOMPARE(resolution.predictedDestination(), german());
    }

    // Nothing detected or translated yet: the fallback, and the preference
    // rule applied to it. This is the answer the UI shows before the first
    // translation, and it is a prediction - not a fact.
    void testPredictionBeforeAnythingHappens()
    {
        LanguageResolution resolution;
        configure(resolution);
        resolution.setSelected(Language::autoLanguage(), Language::autoLanguage());

        QCOMPARE(resolution.effectiveSource(), english());
        // source == primary, so the destination falls through to secondary
        QCOMPARE(resolution.effectiveDestination(), russian());
        QVERIFY(!resolution.translatedSource().has_value());
        QVERIFY(!resolution.translatedDestination().has_value());
    }

    // Detection outranks the fallback, and moves the destination with it.
    void testDetectionMovesBothEnds()
    {
        LanguageResolution resolution;
        configure(resolution);
        resolution.setSelected(Language::autoLanguage(), Language::autoLanguage());
        resolution.setDetectedSource(polish());

        QCOMPARE(resolution.effectiveSource(), polish());
        // source != primary, so the destination is the primary
        QCOMPARE(resolution.effectiveDestination(), english());
    }

    // THE BUG THIS CLASS EXISTS FOR. What the provider actually used outranks
    // any prediction, and is what speech and the voice combos must follow -
    // reading the fallback here is how Russian output was spoken in English.
    void testWhatTheProviderUsedOutranksThePrediction()
    {
        LanguageResolution resolution;
        configure(resolution);
        resolution.setSelected(Language::autoLanguage(), Language::autoLanguage());

        // The prediction before the answer arrives.
        QCOMPARE(resolution.effectiveDestination(), russian());

        resolution.setTranslated(polish(), english());
        QCOMPARE(resolution.effectiveSource(), polish());
        QCOMPARE(resolution.effectiveDestination(), english());
        QCOMPARE(resolution.translatedDestination().value(), english());
    }

    // "Not resolved yet" is a state, not a language. Rendering the prediction
    // as though it were the answer is what every one of these bugs did.
    void testUnresolvedIsDistinguishableFromResolved()
    {
        LanguageResolution resolution;
        configure(resolution);
        resolution.setSelected(Language::autoLanguage(), Language::autoLanguage());
        QVERIFY(!resolution.translatedDestination().has_value());

        resolution.setTranslated(english(), russian());
        QVERIFY(resolution.translatedDestination().has_value());

        // New source text: what the last translation used no longer describes
        // what is on screen, and the class says so rather than keeping it.
        resolution.forgetTranslation();
        QVERIFY(!resolution.translatedDestination().has_value());
        QCOMPARE(resolution.effectiveDestination(), russian()); // back to the prediction
    }

    // A provider that has not said yet reports auto, which must not be stored
    // as though it were an answer.
    void testAutoFromTheProviderIsNotAnAnswer()
    {
        LanguageResolution resolution;
        configure(resolution);
        resolution.setSelected(Language::autoLanguage(), Language::autoLanguage());
        resolution.setTranslated(Language::autoLanguage(), Language::autoLanguage());

        QVERIFY(!resolution.translatedSource().has_value());
        QVERIFY(!resolution.translatedDestination().has_value());
        QCOMPARE(resolution.effectiveDestination(), russian());
    }

    // The signal is what every consumer hangs off, so it has to fire exactly
    // when an output moves - and not when one does not.
    void testChangedFiresOnlyWhenAnAnswerMoves()
    {
        LanguageResolution resolution;
        configure(resolution);
        resolution.setSelected(Language::autoLanguage(), Language::autoLanguage());

        QSignalSpy spy(&resolution, &LanguageResolution::changed);

        resolution.setTranslated(polish(), english());
        QCOMPARE(spy.count(), 1);

        // Same answer again: nothing moved.
        resolution.setTranslated(polish(), english());
        QCOMPARE(spy.count(), 1);

        // A detection that does not change the effective answer, because what
        // the provider used still outranks it.
        resolution.setDetectedSource(polish());
        QCOMPARE(spy.count(), 1);

        resolution.setTranslated(german(), russian());
        QCOMPARE(spy.count(), 2);
    }

    // Changing the preference has to move the prediction, or the auto button
    // and the voice combos go stale after a settings change.
    void testPreferenceChangeMovesThePrediction()
    {
        LanguageResolution resolution;
        configure(resolution);
        resolution.setSelected(Language::autoLanguage(), Language::autoLanguage());
        QCOMPARE(resolution.effectiveDestination(), russian());

        QSignalSpy spy(&resolution, &LanguageResolution::changed);
        resolution.setPreference(english(), german(), english());
        QCOMPARE(spy.count(), 1);
        QCOMPARE(resolution.effectiveDestination(), german());
    }

    // The selection has to survive being read back as it went in. Every
    // resolved output above answers "auto" with a concrete language, on
    // purpose - so a caller that needs to know the user asked for auto,
    // rather than what auto currently resolves to, cannot use them. That is
    // exactly the distinction a translation request depends on: sending the
    // resolved destination pins it, and the retranslate-on-detection path
    // never runs.
    void testSelectionReadsBackUnresolved()
    {
        LanguageResolution resolution;
        configure(resolution);
        resolution.setSelected(Language::autoLanguage(), Language::autoLanguage());
        resolution.setDetectedSource(polish());
        resolution.setTranslated(polish(), english());

        QCOMPARE(resolution.selectedSource(), Language::autoLanguage());
        QCOMPARE(resolution.selectedDestination(), Language::autoLanguage());
        // ... while the resolved answers have moved well away from it.
        QCOMPARE(resolution.effectiveSource(), polish());
        QCOMPARE(resolution.effectiveDestination(), english());

        resolution.setSelected(german(), russian());
        QCOMPARE(resolution.selectedSource(), german());
        QCOMPARE(resolution.selectedDestination(), russian());
    }
};

QTEST_GUILESS_MAIN(LanguageResolutionTest)
#include "test_languageresolution.moc"
