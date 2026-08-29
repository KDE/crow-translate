/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

// The translation view's renderer, which used to live inside
// MozhiTranslationProvider and made that backend responsible for one
// frontend's presentation.
//
// Golden-output tests: the markup moved without changing, so the window looks
// exactly as it did, and that is only true if it is checked character for
// character. Headless - rendering a string needs no display.

#include "core/translationresult.h"
#include "gui/translationhtmlformatter.h"

#include <QTest>

class TranslationHtmlFormatterTest : public QObject
{
    Q_OBJECT

private slots:
    void testPlainTranslationIsEscaped()
    {
        TranslationResult result;
        result.translation = QStringLiteral("5 < 6 & <b>bold</b>");

        // Escaping belongs here, where text becomes markup - not in whichever
        // backend produced the text. Copy used to escape on the way in for
        // exactly this reason, and no longer has to.
        QCOMPARE(TranslationHtmlFormatter::format(result), QStringLiteral("5 &lt; 6 &amp; &lt;b&gt;bold&lt;/b&gt;"));
    }

    void testNewlinesBecomeLineBreaks()
    {
        TranslationResult result;
        result.translation = QStringLiteral("first\nsecond");

        QCOMPARE(TranslationHtmlFormatter::format(result), QStringLiteral("first<br>second"));
    }

    void testTransliterationsKeepTheirOldMarkup()
    {
        TranslationResult result;
        result.translation = QStringLiteral("Hallo Welt");
        result.translationTranslit = QStringLiteral("HALLO VELT");
        result.sourceTranslit = QStringLiteral("helo world");
        result.sourceTranscription = QStringLiteral("hɛˈloʊ");

        QCOMPARE(TranslationHtmlFormatter::format(result),
                 QStringLiteral("Hallo Welt"
                                "<br><font color=\"grey\"><i>/HALLO VELT/</i></font>"
                                "<br><font color=\"grey\"><i><b>(helo world)</b></i></font>"
                                "<br><font color=\"grey\">[hɛˈloʊ]</font>"));
    }

    // Regression guard for a pair of errors that used to cancel out.
    // OnlineTranslator filled TranslationExample - {word, definition,
    // example, ...} - by passing {word, example, definition, ...}, so each
    // field held the other's value; the old formatter then bound them back
    // the wrong way round as well. Correcting either one alone would have
    // swapped the definition and the example on screen without anything
    // failing. Both are fixed; this pins which is which.
    void testDefinitionAndExampleAreNotSwapped()
    {
        TranslationResult result;
        result.translation = QStringLiteral("Katze");

        TranslationExample entry;
        entry.word = QStringLiteral("cat");
        entry.definition = QStringLiteral("a small domesticated carnivore");
        entry.example = QStringLiteral("the cat sat on the mat");
        result.examples = {entry};

        const QString formatted = TranslationHtmlFormatter::format(result);

        // The definition renders plainly, the usage example in grey italics.
        QVERIFY2(formatted.contains(QStringLiteral("&nbsp;&nbsp;&nbsp;&nbsp;a small domesticated carnivore<br>")),
                 qPrintable(QStringLiteral("definition not rendered plainly:\n%1").arg(formatted)));
        QVERIFY2(formatted.contains(QStringLiteral("<font color=\"grey\"><i>the cat sat on the mat</i></font>")),
                 qPrintable(QStringLiteral("example not rendered in grey italics:\n%1").arg(formatted)));
        // The giveaway if they ever swap back.
        QVERIFY2(!formatted.contains(QStringLiteral("<font color=\"grey\"><i>a small domesticated carnivore</i></font>")),
                 "the definition is being rendered where the example belongs");
    }

    void testTranslationOptionsKeepTheirOldMarkup()
    {
        TranslationResult result;
        result.translation = QStringLiteral("Katze");
        result.options = {{QStringLiteral("noun"), {QStringLiteral("Katze"), QStringLiteral("Muschi")}}};

        QCOMPARE(TranslationHtmlFormatter::format(result),
                 QStringLiteral("Katze<br><br><b>translation options:</b><br>"
                                "&nbsp;&nbsp;&nbsp;&nbsp;noun: <font color=\"grey\"><i>Katze, Muschi</i></font><br>"));
    }

    void testEmptyResultRendersNothing()
    {
        QCOMPARE(TranslationHtmlFormatter::format(TranslationResult()), QString());
    }
};

QTEST_GUILESS_MAIN(TranslationHtmlFormatterTest)

#include "test_translationhtmlformatter.moc"
