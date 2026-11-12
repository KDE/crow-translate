/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

// What a request to the desktop portal says its parent window is. The answer
// depends on the platform the test is run on, so the test reads the platform
// and holds the function to what is right there: run offscreen it checks that
// nothing is waited for, run on X11 that the window's id is given, and run as
// the client of a Wayland compositor that the compositor's handle for the
// exported window is - and that it is still the same when asked for again,
// which it was not while the export lived only as long as the question.

#include "xdgdesktopportal.h"

#include <QElapsedTimer>
#include <QGuiApplication>
#include <QTest>
#include <QWindow>

class XdgDesktopPortalTest : public QObject
{
    Q_OBJECT

private:
    static bool onWayland()
    {
        return QGuiApplication::platformName().startsWith(QLatin1String("wayland"));
    }

    static bool onX11()
    {
        return QGuiApplication::platformName() == QLatin1String("xcb");
    }

private slots:
    void noWindowHasNoName()
    {
        QCOMPARE(XdgDesktopPortal::parentWindow(nullptr), QString());
    }

    void windowIsNamedAsThePlatformNamesIt()
    {
        QWindow window;
        window.resize(200, 100);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));

        QElapsedTimer timer;
        timer.start();
        const QString name = XdgDesktopPortal::parentWindow(&window);
        const qint64 took = timer.elapsed();

        if (onX11()) {
            QCOMPARE(name, QStringLiteral("x11:%1").arg(window.winId(), 0, 16));
        } else if (onWayland()) {
#ifdef WITH_KWINDOWSYSTEM
            QVERIFY2(name.startsWith(QLatin1String("wayland:")), qPrintable(name));
            QVERIFY2(name.size() > int(qstrlen("wayland:")), qPrintable(name));
#else
            QCOMPARE(name, QString());
#endif
        } else {
            // A platform with no way of naming a window to another process
            // (offscreen, in the tests). There is nobody to answer, and so
            // nothing to wait for.
            QCOMPARE(name, QString());
            QVERIFY2(took < 500, qPrintable(QStringLiteral("waited %1 ms").arg(took)));
        }
    }

    void nameStaysGood()
    {
        if (!onWayland())
            QSKIP("Only a Wayland window is named by something that can be withdrawn");
#ifndef WITH_KWINDOWSYSTEM
        QSKIP("Built without KWindowSystem: a Wayland window is not named at all");
#endif
        QWindow window;
        window.resize(200, 100);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));

        // The portal uses the handle after the function has returned, so the
        // window has to stay exported: asked again, it is the same handle,
        // and a second window has one of its own.
        const QString first = XdgDesktopPortal::parentWindow(&window);
        const QString again = XdgDesktopPortal::parentWindow(&window);
        QVERIFY2(first.startsWith(QLatin1String("wayland:")), qPrintable(first));
        QCOMPARE(again, first);

        QWindow other;
        other.resize(200, 100);
        other.show();
        QVERIFY(QTest::qWaitForWindowExposed(&other));
        const QString otherName = XdgDesktopPortal::parentWindow(&other);
        QVERIFY2(otherName.startsWith(QLatin1String("wayland:")), qPrintable(otherName));
        QVERIFY(otherName != first);
    }
};

QTEST_MAIN(XdgDesktopPortalTest)
#include "test_xdgdesktopportal.moc"
