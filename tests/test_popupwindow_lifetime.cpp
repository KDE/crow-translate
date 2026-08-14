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

#include <QTest>

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
