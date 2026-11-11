/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

// Pins the canonical auto-destination rule (TranslationLogic) so it can never
// regress in one backend while another is correct. This behaviour has broken
// repeatedly; keep it in one place and covered here.

#include "translator/translationlogic.h"

#include <QtTest>

Q_DECLARE_METATYPE(TranslationLogic::ForcedFlow)

class TranslationLogicTest : public QObject
{
    Q_OBJECT

private slots:
    void sameLanguageTreatsBaseLanguageAsEqual();
    void preferredDestinationPicksPrimaryWhenSourceDiffers();
    void preferredDestinationPicksSecondaryWhenSourceIsPrimary();
    void preferredDestinationFallsBackToSystem();
    void forcingSwitchesTheSidesTheSettingsNameForTheAction_data();
    void forcingSwitchesTheSidesTheSettingsNameForTheAction();
};

void TranslationLogicTest::sameLanguageTreatsBaseLanguageAsEqual()
{
    using TranslationLogic::sameLanguage;
    QVERIFY(sameLanguage(Language(QStringLiteral("en")), Language(QLocale(QStringLiteral("en_US")))));
    QVERIFY(sameLanguage(Language(QStringLiteral("en")), Language(QLocale(QStringLiteral("en_GB")))));
    QVERIFY(sameLanguage(Language(QStringLiteral("pl")), Language(QStringLiteral("pl"))));
    QVERIFY(!sameLanguage(Language(QStringLiteral("en")), Language(QStringLiteral("pl"))));
}

void TranslationLogicTest::preferredDestinationPicksPrimaryWhenSourceDiffers()
{
    using TranslationLogic::preferredDestination;
    const Language primary(QStringLiteral("en"));
    const Language secondary(QStringLiteral("ru"));
    const Language fallback(QLocale(QStringLiteral("fr_FR")));

    QCOMPARE(preferredDestination(Language(QStringLiteral("pl")), primary, secondary, fallback).toCode(), primary.toCode());
    QCOMPARE(preferredDestination(Language(QStringLiteral("de")), primary, secondary, fallback).toCode(), primary.toCode());
}

void TranslationLogicTest::preferredDestinationPicksSecondaryWhenSourceIsPrimary()
{
    using TranslationLogic::preferredDestination;
    const Language primary(QStringLiteral("en"));
    const Language secondary(QStringLiteral("ru"));
    const Language fallback(QLocale(QStringLiteral("fr_FR")));

    QCOMPARE(preferredDestination(primary, primary, secondary, fallback).toCode(), secondary.toCode());
    // a territory variant of the primary is still the primary -> secondary
    QCOMPARE(preferredDestination(Language(QLocale(QStringLiteral("en_US"))), primary, secondary, fallback).toCode(),
             secondary.toCode());
    // source equal to the secondary -> primary
    QCOMPARE(preferredDestination(secondary, primary, secondary, fallback).toCode(), primary.toCode());
}

void TranslationLogicTest::preferredDestinationFallsBackToSystem()
{
    using TranslationLogic::preferredDestination;
    const Language primary(QStringLiteral("en"));
    const Language fallback(QLocale(QStringLiteral("fr_FR")));

    // unset primary and secondary -> fallback
    QCOMPARE(preferredDestination(primary, Language::autoLanguage(), Language::autoLanguage(), fallback).toCode(),
             fallback.toCode());
    // source equals both primary and secondary -> fallback
    QCOMPARE(preferredDestination(primary, primary, primary, fallback).toCode(), fallback.toCode());
}

// What a global action does to the language buttons, as a table. The two
// settings are on as installed; every test and every harness used to turn
// them off first, so none of this was pinned anywhere.
void TranslationLogicTest::forcingSwitchesTheSidesTheSettingsNameForTheAction_data()
{
    using TranslationLogic::ForcedFlow;
    QTest::addColumn<ForcedFlow>("flow");
    QTest::addColumn<bool>("forceSource");
    QTest::addColumn<bool>("forceDestination");
    QTest::addColumn<bool>("engineDetects");
    QTest::addColumn<bool>("source");
    QTest::addColumn<bool>("destination");

    // As installed, with a backend that can find a language out.
    QTest::newRow("translate selection: both sides") << ForcedFlow::TranslateSelection << true << true << true << true << true;
    QTest::newRow("recognition: both sides") << ForcedFlow::Recognition << true << true << true << true << true;
    QTest::newRow("speak selection: there is no destination") << ForcedFlow::SpeakSelection << true << true << true << true << false;

    // A backend that cannot detect has no use for an automatic source and
    // fails on one (DeepL, Reverso; Copy). The chosen source stays; the
    // destination is forced as before and follows from that source.
    QTest::newRow("no detection, translate selection") << ForcedFlow::TranslateSelection << true << true << false << false << true;
    QTest::newRow("no detection, recognition") << ForcedFlow::Recognition << true << true << false << false << true;
    QTest::newRow("no detection, speak selection") << ForcedFlow::SpeakSelection << true << true << false << false << false;

    // Each setting is its own side's.
    QTest::newRow("source only") << ForcedFlow::TranslateSelection << true << false << true << true << false;
    QTest::newRow("destination only") << ForcedFlow::TranslateSelection << false << true << true << false << true;
    QTest::newRow("destination only, recognition") << ForcedFlow::Recognition << false << true << true << false << true;
    QTest::newRow("destination only, speak selection") << ForcedFlow::SpeakSelection << false << true << true << false << false;
    QTest::newRow("neither") << ForcedFlow::TranslateSelection << false << false << true << false << false;
    QTest::newRow("neither, recognition") << ForcedFlow::Recognition << false << false << true << false << false;
}

void TranslationLogicTest::forcingSwitchesTheSidesTheSettingsNameForTheAction()
{
    QFETCH(TranslationLogic::ForcedFlow, flow);
    QFETCH(bool, forceSource);
    QFETCH(bool, forceDestination);
    QFETCH(bool, engineDetects);
    QFETCH(bool, source);
    QFETCH(bool, destination);

    const TranslationLogic::ForcedAuto forced = TranslationLogic::forcedAuto(flow, forceSource, forceDestination, engineDetects);
    QCOMPARE(forced.source, source);
    QCOMPARE(forced.destination, destination);
}

QTEST_GUILESS_MAIN(TranslationLogicTest)
#include "test_translationlogic.moc"