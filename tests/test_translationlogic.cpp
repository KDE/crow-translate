/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

// Pins the canonical auto-destination rule (TranslationLogic) so it can never
// regress in one backend while another is correct. This behaviour has broken
// repeatedly; keep it in one place and covered here.

#include "translator/translationlogic.h"

#include <QtTest>

class TranslationLogicTest : public QObject
{
    Q_OBJECT

private slots:
    void sameLanguageTreatsBaseLanguageAsEqual();
    void preferredDestinationPicksPrimaryWhenSourceDiffers();
    void preferredDestinationPicksSecondaryWhenSourceIsPrimary();
    void preferredDestinationFallsBackToSystem();
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

QTEST_GUILESS_MAIN(TranslationLogicTest)
#include "test_translationlogic.moc"