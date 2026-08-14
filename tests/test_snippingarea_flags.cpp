/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

// Regression test for gotcha #11: on Wayland, Qt::Popup requires a visible
// transient parent that has received input - if the parent is hidden, popup
// creation silently fails. Fixed (97de84dc, branch fix-popupwindow-wayland)
// by falling back to Qt::Tool when the parent isn't visible. This tests the
// decision function directly (SnippingArea::getWindowFlags), not the whole
// snipping/OCR flow, so it's fast and deterministic - and since it reads the
// real QGuiApplication::platformName() at runtime, it exercises the actual
// Wayland branch whenever run on a real Wayland session (as opposed to only
// asserting the always-safe non-Wayland behavior).

#include "singleapplication.h"
#include "testisolation.h"
#include "ocr/snippingarea.h"

#include <QGuiApplication>
#include <QTest>
#include <QWidget>

class SnippingAreaFlagsTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        if (qgetenv("DISPLAY").isEmpty() && qgetenv("WAYLAND_DISPLAY").isEmpty())
            QSKIP("No display server available - skipping GUI tests");
    }

    // Qt::WindowType values are composite bit flags, not mutually-exclusive
    // enumerators - Qt::Tool is literally defined as `Popup | Dialog`, so a
    // plain `flags & Qt::Popup` membership test reads true for BOTH a Popup
    // window AND a Tool window, and can't tell them apart. Extracting the
    // type via Qt::WindowType_Mask before comparing is what actually
    // distinguishes them.
    static Qt::WindowType windowType(Qt::WindowFlags flags)
    {
        return static_cast<Qt::WindowType>(static_cast<int>(flags & Qt::WindowType_Mask));
    }

    void testNullParentAlwaysPopup()
    {
        // No parent to be hidden/invisible - always the plain Popup flag.
        QCOMPARE(windowType(SnippingArea::getWindowFlags(nullptr)), Qt::Popup);
    }

    void testHiddenParentBehaviorMatchesPlatform()
    {
        QWidget parent;
        parent.hide();

        const Qt::WindowFlags flags = SnippingArea::getWindowFlags(&parent);
        const bool isWayland = QGuiApplication::platformName() == QLatin1String("wayland");

        if (isWayland) {
            QCOMPARE(windowType(flags), Qt::Tool);
        } else {
            QCOMPARE(windowType(flags), Qt::Popup);
        }
    }

    void testVisibleParentAlwaysPopup()
    {
        QWidget parent;
        parent.show();
        QVERIFY(QTest::qWaitForWindowExposed(&parent));

        const Qt::WindowFlags flags = SnippingArea::getWindowFlags(&parent);
        QCOMPARE(windowType(flags), Qt::Popup);
    }
};

int main(int argc, char *argv[])
{
    isolateTestSettings();

    SingleApplication app(argc, argv, true);
    SnippingAreaFlagsTest tc;
    QTEST_SET_MAIN_SOURCE_PATH
    return QTest::qExec(&tc, argc, argv);
}

#include "test_snippingarea_flags.moc"
