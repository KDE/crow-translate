/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

// Regression test for PopupWindow's parent-lifetime handling (gotcha #10 in
// this project's history: this exact bug has round-tripped fixed -> regressed
// -> fixed once already, see commits 97de84dc / 49675592 / a99efe2f and their
// later re-application). PopupWindow connects to
// parent->translationEdit()->textChanged with a lambda that captures `this`
// via the 3-argument connect(sender, signal, functor) overload - which has NO
// context object, so Qt cannot auto-sever the connection when the popup is
// destroyed. ~PopupWindow() must explicitly disconnect m_textChangedConnection,
// or a later textChanged emission fires the lambda with a dangling `this`.
//
// This test intentionally does not assert on a captured value for "did it
// crash" - if the destructor's disconnect regresses, this test process
// crashes/UB's out before reaching the final QVERIFY, which is the standard
// way this class of lifetime bug is caught in a unit test.

#include "mainwindow.h"
#include "popupwindow.h"
#include "singleapplication.h"
#include "testisolation.h"
#include "translationedit.h"
#include "settings/appsettings.h"

#include <QApplication>
#include <QLayout>
#include <QSizeGrip>
#include <QTest>
#include <QTimer>

class PopupWindowLifetimeTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        if (qgetenv("DISPLAY").isEmpty() && qgetenv("WAYLAND_DISPLAY").isEmpty())
            QSKIP("No display server available - skipping GUI tests");
    }

    void testPopupSurvivesAfterOwnDestructionWithoutDanglingConnection()
    {
        AppSettings settings;
        settings.setWindowMode(AppSettings::PopupWindow);

        auto *window = new MainWindow();
        QVERIFY(window->translationEdit() != nullptr);
        window->translationEdit()->setHtml(QStringLiteral("before"));

        auto *popup = new PopupWindow(window);
        QTest::qWait(50);

        // Connection is live: parent's edit changing should propagate to the popup.
        window->translationEdit()->setHtml(QStringLiteral("while popup alive"));
        QTest::qWait(50);

        // Destroy only the popup (this is what WA_DeleteOnClose does on close()) -
        // its destructor must disconnect m_textChangedConnection.
        delete popup;

        // If the disconnect didn't happen, this emission fires the lambda with
        // a dangling `this` - crash/UB, which fails this test by aborting the
        // process rather than by a QVERIFY.
        window->translationEdit()->setHtml(QStringLiteral("after popup destroyed"));
        QTest::qWait(50);

        delete window;

        QVERIFY(true);
    }
    // The pop-up is frameless (Qt::Tool | Qt::FramelessWindowHint), so it has no
    // edge of its own to grab: before this it could only be sized from the
    // settings dialog, and a size it was dragged to would not have been kept. A
    // grip over the status strip's corner resizes it, and the size it is left at
    // goes back to the settings, which is what the next pop-up opens with.
    //
    // The write-out is debounced (a drag emits a resize per frame), and the pop-up
    // closes itself when it loses activation - so the timer is asserted on its own
    // and the flush is asserted through the close, which a drag cannot beat.
    void testPopupRemembersTheSizeItWasResizedTo()
    {
        MainWindow window;
        PopupWindow popup(&window);
        popup.show();
        QVERIFY(QTest::qWaitForWindowExposed(&popup));

        // The grip is not in a layout: unsized, it would be a 100x30 default
        // widget raised over the buttons in that corner.
        auto *grip = popup.findChild<QSizeGrip *>();
        QVERIFY2(grip != nullptr, "a frameless window needs a grip to be resizable");
        QCOMPARE(grip->size(), grip->sizeHint());
        QCOMPARE(grip->geometry().bottomRight(), popup.rect().bottomRight());

        // The layout's minimum is the floor: an explicit smaller one would let
        // a drag (or a small configured size) squeeze the controls.
        const QSize layoutMinimum = popup.layout()->totalMinimumSize();
        QVERIFY(popup.minimumWidth() >= layoutMinimum.width());
        QVERIFY(popup.minimumHeight() >= layoutMinimum.height());

        const QSize dragged = popup.size() + QSize(60, 40);
        popup.resize(dragged);

        auto *saveTimer = popup.findChild<QTimer *>(QStringLiteral("popupSizeSaveTimer"));
        QVERIFY2(saveTimer != nullptr, "the size write-out timer should exist");
        QVERIFY2(saveTimer->isActive(), "resizing should schedule the write-out");

        // Closing before the debounce fires keeps the size the user dragged to.
        popup.close();
        QCOMPARE(AppSettings().popupWidth(), dragged.width());
        QCOMPARE(AppSettings().popupHeight(), dragged.height());

        AppSettings().setPopupWidth(AppSettings::defaultPopupWidth());
        AppSettings().setPopupHeight(AppSettings::defaultPopupHeight());
    }

    // Only a drag is the user's choice. The resize a pop-up is shown with is the
    // configured size, or the layout's minimum when that is larger, and opening
    // the pop-up must not write that back over the setting.
    void testOpeningThePopupLeavesTheSizeSettingAlone()
    {
        const AppSettings settings;
        const int width = settings.popupWidth();
        const int height = settings.popupHeight();

        MainWindow window;
        PopupWindow popup(&window);
        popup.show();
        QVERIFY(QTest::qWaitForWindowExposed(&popup));

        auto *saveTimer = popup.findChild<QTimer *>(QStringLiteral("popupSizeSaveTimer"));
        QVERIFY(saveTimer != nullptr);
        QVERIFY2(!saveTimer->isActive(), "showing the pop-up is not a resize by the user");

        popup.close();
        QCOMPARE(AppSettings().popupWidth(), width);
        QCOMPARE(AppSettings().popupHeight(), height);
    }

    // The auto-close timeout stops while the pointer is over the pop-up. A resize
    // pushes back a timeout that is running, so it cannot fire mid-drag, but it
    // must not start one the pointer has stopped - the window would then close
    // under the cursor of someone reading it.
    void testResizingDoesNotStartTheTimeoutWhileHovered()
    {
        AppSettings().setPopupWindowTimeout(30);

        MainWindow window;
        PopupWindow popup(&window);
        popup.show();
        QVERIFY(QTest::qWaitForWindowExposed(&popup));

        auto *closeTimer = popup.findChild<QTimer *>(QStringLiteral("popupCloseTimer"));
        QVERIFY(closeTimer != nullptr);

        QEvent enter(QEvent::Enter);
        QApplication::sendEvent(&popup, &enter);
        QVERIFY(!closeTimer->isActive());
        popup.resize(popup.size() + QSize(20, 20));
        QVERIFY2(!closeTimer->isActive(), "a resize under the pointer must not start the timeout");

        QEvent leave(QEvent::Leave);
        QApplication::sendEvent(&popup, &leave);
        QVERIFY(closeTimer->isActive());
        popup.resize(popup.size() + QSize(20, 20));
        QVERIFY2(closeTimer->isActive(), "a running timeout is pushed back, not dropped");

        AppSettings().setPopupWindowTimeout(AppSettings::defaultPopupWindowTimeout());
        AppSettings().setPopupWidth(AppSettings::defaultPopupWidth());
        AppSettings().setPopupHeight(AppSettings::defaultPopupHeight());
        popup.close();
    }
};

int main(int argc, char *argv[])
{
    isolateTestSettings();

    SingleApplication app(argc, argv, true);
    PopupWindowLifetimeTest tc;
    QTEST_SET_MAIN_SOURCE_PATH
    return QTest::qExec(&tc, argc, argv);
}

#include "test_popupwindow_lifetime.moc"
