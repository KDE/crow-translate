/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

// Regression test for gotcha #9: if the app crashes/is killed, SingleApplication's
// shared-memory/socket lock could be left in a stale state that falsely made a
// later relaunch think a primary instance was still running (or spawn a broken
// one). Fixed upstream in SingleApplication v3.5.6 (itay-grudev/SingleApplication#217),
// which verifies the recorded primary PID is still alive with kill(pid, 0) /
// OpenProcess() before treating it as running, and otherwise takes over as the
// new primary instance.
//
// This drives the REAL built `crow` binary via QProcess (not the library, since
// the bug is about OS-level shared-memory/socket recovery, which only a real
// second process launch actually exercises), SIGKILLs it to simulate a hard
// crash, then relaunches and asserts the new process becomes primary and stays
// running rather than hanging or exiting as a bogus "secondary" instance.
//
// SingleApplication's instance identity is derived from the app/organization
// name, not from $XDG_CONFIG_HOME/$HOME, so it cannot be isolated per-test-run
// via environment alone without changing the app's identity in main.cpp - which
// would defeat the point of testing the real binary. If a real crow-translate
// instance already owns the D-Bus service name, this test skips rather than
// risk interfering with someone's actual running session.

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <QTest>

#ifndef CROW_BINARY_PATH
#error "CROW_BINARY_PATH must be defined by CMake to the built crow executable path"
#endif

class SingleApplicationRecoveryTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        if (qgetenv("DISPLAY").isEmpty() && qgetenv("WAYLAND_DISPLAY").isEmpty())
            QSKIP("No display server available - skipping GUI tests");

        if (QDBusConnection::sessionBus().interface()->isServiceRegistered(QStringLiteral("org.kde.CrowTranslate")))
            QSKIP("A crow-translate instance is already running on this session - skipping to avoid interfering with it");
    }

    void testRelaunchAfterHardKillBecomesPrimary()
    {
        // Isolate only the settings location, not the process identity - the
        // real crow binary's own screenshot-permission grant (KWin's
        // ScreenShot2, tied to its .desktop file identity/executable path,
        // not to any env var) must keep working exactly as it does outside
        // tests. Same binary, same path, same QGuiApplication::desktopFileName()
        // - just a scratch XDG_CONFIG_HOME so this doesn't read/write the
        // real user's ~/.config/crow-translate/.
        QTemporaryDir configDir;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(QStringLiteral("XDG_CONFIG_HOME"), configDir.path());

        // A genuinely empty config dir makes the real binary hit its actual
        // first-run behavior: MainWindow::loadMainWindowSettings() blocks on
        // a privacy QMessageBox::exec(), and launchGui() blocks on an
        // InstancePingerDialog before that if no instance is configured -
        // both would sit there forever with nobody to click through them.
        // Pre-seed just enough real settings (same file the app itself
        // would write) to skip both, so this test still launches the actual
        // production binary/config-loading path, just past its one-time
        // setup dialogs.
        QDir(configDir.path()).mkpath(QStringLiteral("crow-translate"));
        QFile presetConfig(configDir.filePath(QStringLiteral("crow-translate/crow-translate.conf")));
        QVERIFY(presetConfig.open(QIODevice::WriteOnly | QIODevice::Text));
        presetConfig.write(QByteArrayLiteral(
            "[MainWindow]\n"
            "ShowPrivacyPopup=false\n"
            "\n"
            "[Translation]\n"
            "Instance=https://mozhi.aryak.me\n"));
        presetConfig.close();

        QProcess first;
        first.setProcessEnvironment(env);
        first.start(QStringLiteral(CROW_BINARY_PATH));
        QVERIFY(first.waitForStarted(5000));

        QVERIFY2(QTest::qWaitFor(
                     [] {
                         return QDBusConnection::sessionBus().interface()->isServiceRegistered(QStringLiteral("org.kde.CrowTranslate"));
                     },
                     10000),
                 "First instance never registered its D-Bus service - never became primary");

        // Simulate a hard crash: no graceful shutdown, no chance to release
        // SingleApplication's shared-memory/socket lock cleanly.
        first.kill();
        QVERIFY(first.waitForFinished(5000));

        QVERIFY2(QTest::qWaitFor(
                     [] {
                         return !QDBusConnection::sessionBus().interface()->isServiceRegistered(QStringLiteral("org.kde.CrowTranslate"));
                     },
                     5000),
                 "D-Bus service name was not released after the hard kill");

        QProcess second;
        second.setProcessEnvironment(env);
        second.start(QStringLiteral(CROW_BINARY_PATH));
        QVERIFY(second.waitForStarted(5000));

        const bool becamePrimary = QTest::qWaitFor(
            [] {
                return QDBusConnection::sessionBus().interface()->isServiceRegistered(QStringLiteral("org.kde.CrowTranslate"));
            },
            10000);

        second.kill();
        second.waitForFinished(5000);

        QVERIFY2(becamePrimary, "Relaunch after a hard kill did not become primary within 10s - stale SingleApplication lock blocked recovery");
    }
};

int main(int argc, char *argv[])
{
    SingleApplicationRecoveryTest tc;
    QTEST_SET_MAIN_SOURCE_PATH
    QCoreApplication app(argc, argv);
    return QTest::qExec(&tc, argc, argv);
}

#include "test_singleapplication_recovery.moc"
