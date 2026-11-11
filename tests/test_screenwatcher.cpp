/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

// The main window lays itself out for an orientation: the one chosen in the settings, or the
// screen's. Not every platform knows how its screen is turned. Qt's offscreen and VNC platforms
// and a plain framebuffer answer Qt::PrimaryOrientation to that question too, and the window had
// no layout for that answer: it ran into Q_UNREACHABLE(), which in a release build left the row of
// buttons under the translation without a direction (all of them in the window's top left corner,
// on top of the language buttons) and in a debug build aborts.

#include "screenwatcher.h"

#include <QtTest>

class ScreenWatcherTest : public QObject
{
    Q_OBJECT

private slots:
    void anOrientationChosenInTheSettingsIsKept_data();
    void anOrientationChosenInTheSettingsIsKept();
    void leftToTheScreenItIsTheScreens_data();
    void leftToTheScreenItIsTheScreens();
    void aScreenThatDoesNotSayIsJudgedByItsShape();
    void aValueThatIsNoOrientationIsLeftToTheScreen();
    void theAnswerIsAlwaysOneTheWindowHasALayoutFor();
};

static const QList<Qt::ScreenOrientation> real = {Qt::LandscapeOrientation,
                                                  Qt::PortraitOrientation,
                                                  Qt::InvertedLandscapeOrientation,
                                                  Qt::InvertedPortraitOrientation};

void ScreenWatcherTest::anOrientationChosenInTheSettingsIsKept_data()
{
    QTest::addColumn<Qt::ScreenOrientation>("chosen");
    QTest::newRow("landscape") << Qt::LandscapeOrientation;
    QTest::newRow("portrait") << Qt::PortraitOrientation;
    QTest::newRow("inverted landscape") << Qt::InvertedLandscapeOrientation;
    QTest::newRow("inverted portrait") << Qt::InvertedPortraitOrientation;
}

void ScreenWatcherTest::anOrientationChosenInTheSettingsIsKept()
{
    QFETCH(Qt::ScreenOrientation, chosen);
    // Whatever the screen says, and whatever its shape.
    for (const Qt::ScreenOrientation reported : real + QList<Qt::ScreenOrientation>{Qt::PrimaryOrientation}) {
        QCOMPARE(ScreenWatcher::layoutOrientation(chosen, reported, Qt::LandscapeOrientation), chosen);
        QCOMPARE(ScreenWatcher::layoutOrientation(chosen, reported, Qt::PortraitOrientation), chosen);
    }
}

void ScreenWatcherTest::leftToTheScreenItIsTheScreens_data()
{
    anOrientationChosenInTheSettingsIsKept_data();
}

void ScreenWatcherTest::leftToTheScreenItIsTheScreens()
{
    QFETCH(Qt::ScreenOrientation, chosen); // here: what the screen reports
    QCOMPARE(ScreenWatcher::layoutOrientation(Qt::PrimaryOrientation, chosen, Qt::LandscapeOrientation), chosen);
    QCOMPARE(ScreenWatcher::layoutOrientation(Qt::PrimaryOrientation, chosen, Qt::PortraitOrientation), chosen);
}

void ScreenWatcherTest::aScreenThatDoesNotSayIsJudgedByItsShape()
{
    // The case that was not handled: "ask the screen", and the screen has no answer.
    QCOMPARE(ScreenWatcher::layoutOrientation(Qt::PrimaryOrientation, Qt::PrimaryOrientation, Qt::LandscapeOrientation),
             Qt::LandscapeOrientation);
    QCOMPARE(ScreenWatcher::layoutOrientation(Qt::PrimaryOrientation, Qt::PrimaryOrientation, Qt::PortraitOrientation),
             Qt::PortraitOrientation);
}

void ScreenWatcherTest::aValueThatIsNoOrientationIsLeftToTheScreen()
{
    // The setting is an integer in a file; 3 is two flags at once and no orientation.
    const auto nonsense = static_cast<Qt::ScreenOrientation>(3);
    QCOMPARE(ScreenWatcher::layoutOrientation(nonsense, Qt::PortraitOrientation, Qt::LandscapeOrientation), Qt::PortraitOrientation);
    QCOMPARE(ScreenWatcher::layoutOrientation(nonsense, Qt::PrimaryOrientation, Qt::LandscapeOrientation), Qt::LandscapeOrientation);
}

void ScreenWatcherTest::theAnswerIsAlwaysOneTheWindowHasALayoutFor()
{
    QList<Qt::ScreenOrientation> any = real;
    any << Qt::PrimaryOrientation << static_cast<Qt::ScreenOrientation>(3) << static_cast<Qt::ScreenOrientation>(16);
    for (const Qt::ScreenOrientation wanted : any) {
        for (const Qt::ScreenOrientation reported : any) {
            for (const Qt::ScreenOrientation primary : any) {
                const Qt::ScreenOrientation answer = ScreenWatcher::layoutOrientation(wanted, reported, primary);
                QVERIFY2(real.contains(answer), qPrintable(QStringLiteral("%1/%2/%3 -> %4").arg(wanted).arg(reported).arg(primary).arg(answer)));
            }
        }
    }
}

QTEST_APPLESS_MAIN(ScreenWatcherTest)
#include "test_screenwatcher.moc"
