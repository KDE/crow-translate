/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

// View-contract tests for StatusStrip. The strip is never shown; all
// visibility assertions use isHidden()/!isHidden(), which reflect the
// explicit show/hide calls synchronously without depending on window
// exposure. Busy/error states are driven through ModuleStatus with providers
// that hold those states persistently (screen capture via beginScreenCapture,
// a synchronous Copy translation error), so no timing races are involved -
// except the ellipsis animation, which is the one thing that genuinely needs
// the timer and gets a qWaitFor().

#include "language.h"
#include "modulestatus.h"
#include "statusstrip.h"
#include "ocr/screengrabbers/genericscreengrabber.h"
#include "ocr/snippingarea.h"
#include "translator/copytranslationprovider.h"
#include "tts/noopttsprovider.h"

#include <QApplication>
#include <QLabel>
#include <QLocale>
#include <QTest>

class StatusStripTest : public QObject
{
    Q_OBJECT

private slots:
    void testReadyAtRest();
    void testBusySegment();
    void testErrorSegmentShowsDetailInTooltip();
    void testSimultaneousBusyAndErrorSegments();
    void testHideWhenIdle();
    void testSetShownOverridesModel();
    void testTtsSegmentOmittedForNoopBackend();
    void testAnimatedEllipsisWhileBusy();
    void testDotsStopWhenIdle();
    void testHiddenStripHidesItsLabels();

private:
    static constexpr auto s_snipping = ModuleStatus::Module::Snipping;
    static constexpr auto s_translating = ModuleStatus::Module::Translation;

    static QLabel *segmentLabel(const StatusStrip *strip, ModuleStatus::Module module);
    static void driveTranslationError(ModuleStatus &model);
};

void StatusStripTest::testReadyAtRest()
{
    ModuleStatus model;
    StatusStrip strip;
    strip.setModel(&model);

    // Not hide-when-idle (the main window configuration): the strip is up
    // with its "Ready" label and every module segment hidden.
    QVERIFY(!strip.isHidden());
    QVERIFY(!strip.findChild<QLabel *>(QStringLiteral("readyLabel"))->isHidden());
    for (int i = 0; i < ModuleStatus::moduleCount(); ++i)
        QVERIFY(segmentLabel(&strip, static_cast<ModuleStatus::Module>(i))->isHidden());
}

void StatusStripTest::testBusySegment()
{
    ModuleStatus model;
    StatusStrip strip;
    strip.setModel(&model);

    model.beginScreenCapture();

    QLabel *ready = strip.findChild<QLabel *>(QStringLiteral("readyLabel"));
    QLabel *segment = segmentLabel(&strip, s_snipping);
    QVERIFY(ready->isHidden());
    QVERIFY(!segment->isHidden());
    QCOMPARE(segment->text(), QStringLiteral("Waiting for capture"));
}

void StatusStripTest::testErrorSegmentShowsDetailInTooltip()
{
    ModuleStatus model;
    StatusStrip strip;
    strip.setModel(&model);

    driveTranslationError(model);

    QLabel *segment = segmentLabel(&strip, s_translating);
    QVERIFY(!segment->isHidden());
    QCOMPARE(segment->text(), QStringLiteral("Translation failed"));
    QVERIFY(!segment->toolTip().isEmpty());
}

void StatusStripTest::testSimultaneousBusyAndErrorSegments()
{
    ModuleStatus model;
    StatusStrip strip;
    strip.setModel(&model);

    model.beginScreenCapture();
    driveTranslationError(model);

    QVERIFY(!segmentLabel(&strip, s_snipping)->isHidden());
    QVERIFY(!segmentLabel(&strip, s_translating)->isHidden());
    QCOMPARE(segmentLabel(&strip, s_snipping)->text(), QStringLiteral("Waiting for capture"));
    QCOMPARE(segmentLabel(&strip, s_translating)->text(), QStringLiteral("Translation failed"));
}

// The pop-up window configuration: strip hidden at rest, up while anything is
// active (busy or error - an error must be visible too, not just work).
void StatusStripTest::testHideWhenIdle()
{
    ModuleStatus model;
    GenericScreenGrabber grabber;
    model.bindCapture(&grabber, nullptr);

    StatusStrip strip;
    strip.setModel(&model);
    strip.setHideWhenIdle(true);

    QVERIFY(strip.isHidden());

    model.beginScreenCapture();
    QVERIFY(!strip.isHidden());

    emit grabber.grabbingFailed();
    QVERIFY(!strip.isHidden());
}

// Interface/ShowStatusBar pick-up: a strip hidden by the setting stays hidden
// whatever the model does, and comes back when re-enabled.
void StatusStripTest::testSetShownOverridesModel()
{
    ModuleStatus model;
    StatusStrip strip;
    strip.setModel(&model);

    strip.setShown(false);
    QVERIFY(strip.isHidden());

    model.beginScreenCapture();
    QVERIFY(strip.isHidden());

    strip.setShown(true);
    QVERIFY(!strip.isHidden());
    QVERIFY(!segmentLabel(&strip, s_snipping)->isHidden());
}

void StatusStripTest::testTtsSegmentOmittedForNoopBackend()
{
    ModuleStatus model;
    NoopTTSProvider noop;
    model.bindTtsProvider(&noop);

    StatusStrip strip;
    strip.setModel(&model);

    // Something else busy, TTS idle: only that segment shows. The TTS label
    // must never appear for the no-op backend (isAvailable == false).
    model.beginScreenCapture();

    QVERIFY(!segmentLabel(&strip, s_snipping)->isHidden());
    QVERIFY(segmentLabel(&strip, ModuleStatus::Module::Tts)->isHidden());
}

void StatusStripTest::testAnimatedEllipsisWhileBusy()
{
    ModuleStatus model;
    StatusStrip strip;
    strip.setModel(&model);

    model.beginScreenCapture();
    const QLabel *segment = segmentLabel(&strip, s_snipping);
    QCOMPARE(segment->text(), QStringLiteral("Waiting for capture"));

    // ~400 ms per dot; give it ample room rather than an exact count, since
    // the assertion is "dots appear and grow", not "exactly N fired".
    QVERIFY(QTest::qWaitFor([&segment]() {
        const QString text = segment->text();
        return text.startsWith(QStringLiteral("Waiting for capture")) && text.endsWith(QLatin1Char('.'));
    },
                            3000));
    QVERIFY(QTest::qWaitFor([&segment]() {
        return segment->text().endsWith(QStringLiteral(".."));
    },
                            3000));

    // The animated ellipsis must not rename the widget for exact-match
    // lookups (AT-SPI Name is what read_widget_text matches on): the
    // accessible name stays the dot-free message while the dots animate.
    QCOMPARE(segment->accessibleName(), QStringLiteral("Waiting for capture"));
}

void StatusStripTest::testDotsStopWhenIdle()
{
    ModuleStatus model;
    GenericScreenGrabber grabber;
    SnippingArea snippingArea;
    model.bindCapture(&grabber, &snippingArea);

    StatusStrip strip;
    strip.setModel(&model);

    model.beginScreenCapture();
    QTest::qWait(900); // let some dots animate

    emit snippingArea.cancelled();
    const QLabel *segment = segmentLabel(&strip, s_snipping);
    QVERIFY(segment->isHidden());
    QVERIFY(segment->text().isEmpty());
    QVERIFY(segment->accessibleName().isEmpty());

    // No further ticks: the state stays clean after the timer's last slot.
    QTest::qWait(900);
    QVERIFY(segment->text().isEmpty());
}

QLabel *StatusStripTest::segmentLabel(const StatusStrip *strip, ModuleStatus::Module module)
{
    static const char *const segmentNames[] = {"snippingSegmentLabel", "ocrSegmentLabel", "translationSegmentLabel", "ttsSegmentLabel"};
    return strip->findChild<QLabel *>(QString::fromLatin1(segmentNames[static_cast<int>(module)]));
}

void StatusStripTest::driveTranslationError(ModuleStatus &model)
{
    CopyTranslationProvider translator;
    model.bindTranslator(&translator);
    translator.translate(QStringLiteral("hello"), Language(QLocale::English), Language(QLocale::French));
}

// A strip that goes away (hide-when-idle at rest, or the ShowStatusBar
// setting) must hide and DE-IDENTIFY its labels, not just hide the
// container: the AT-SPI bridge never prunes interfaces, so a label keeps
// its name-matchable identity (and keeps announcing to screen readers)
// unless it is cleared and hidden itself.
void StatusStripTest::testHiddenStripHidesItsLabels()
{
    ModuleStatus model;
    StatusStrip strip;
    strip.setModel(&model);

    QLabel *ready = strip.findChild<QLabel *>(QStringLiteral("readyLabel"));

    // Hide-when-idle at rest: strip hidden, "Ready" label hidden and
    // de-identified.
    strip.setHideWhenIdle(true);
    QVERIFY(strip.isHidden());
    QVERIFY(ready->isHidden());
    QVERIFY(ready->text().isEmpty());
    QVERIFY(ready->accessibleName().isEmpty());

    // The setting pick-up path: strip suppressed by setShown(false) even
    // while the model is busy.
    strip.setHideWhenIdle(false);
    strip.setShown(false);
    model.beginScreenCapture();
    QVERIFY(strip.isHidden());
    for (int i = 0; i < ModuleStatus::moduleCount(); ++i)
        QVERIFY(segmentLabel(&strip, static_cast<ModuleStatus::Module>(i))->isHidden());

    // Coming back re-shows what the state warrants.
    strip.setShown(true);
    QVERIFY(!strip.isHidden());
    QVERIFY(!segmentLabel(&strip, ModuleStatus::Module::Snipping)->isHidden());
    QVERIFY(ready->isHidden());
    QVERIFY(ready->text().isEmpty());
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    StatusStripTest tc;
    QTEST_SET_MAIN_SOURCE_PATH
    return QTest::qExec(&tc, argc, argv);
}

#include "test_statusstrip.moc"
