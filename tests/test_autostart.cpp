/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

// Autostart on a desktop without the portal (which is every install that is
// not a Flatpak) is the application's desktop file, copied into
// ~/.config/autostart. The copy was taken from /usr/share/applications and
// from nowhere else, so switching autostart on failed for an installation
// anywhere else - /usr/local, /opt, a Nix profile, an AppImage, a build tree:
// "Unable to copy /usr/share/applications/org.kde.CrowTranslate.desktop".
//
// The desktop file is found the way the desktop finds it, along the XDG data
// directories. Headless: no window, no message box on the paths tested here.

#include "settings/autostartmanager/unixautostartmanager.h"

#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QTemporaryDir>
#include <QTest>

class AutostartTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_root;

    QString installedIn(const QString &prefix, const QByteArray &content) const
    {
        const QString applications = m_root.filePath(prefix + QStringLiteral("/share/applications"));
        QDir().mkpath(applications);
        const QString path = applications + QStringLiteral("/org.kde.CrowTranslate.desktop");
        QFile file(path);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(content);
        }
        return path;
    }

    QString autostartFile() const
    {
        return m_root.filePath(QStringLiteral("config/autostart/org.kde.CrowTranslate.desktop"));
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_root.isValid());
        qputenv("XDG_CONFIG_HOME", m_root.filePath(QStringLiteral("config")).toUtf8());
        qputenv("XDG_DATA_HOME", m_root.filePath(QStringLiteral("home-data")).toUtf8());
        // Two prefixes, neither of them /usr; the application is in the second.
        qputenv("XDG_DATA_DIRS",
                (m_root.filePath(QStringLiteral("empty/share")) + QLatin1Char(':') + m_root.filePath(QStringLiteral("opt/crow/share"))).toUtf8());
        QDir().mkpath(m_root.filePath(QStringLiteral("empty/share/applications")));
        QGuiApplication::setDesktopFileName(QStringLiteral("org.kde.CrowTranslate"));
    }

    void cleanup()
    {
        QFile::remove(autostartFile());
    }

    void testTheDesktopFileIsTakenFromWhereTheApplicationIsInstalled()
    {
        installedIn(QStringLiteral("opt/crow"), "[Desktop Entry]\nName=Crow Translate\nExec=/opt/crow/bin/crow\n");

        UnixAutostartManager manager;
        QVERIFY(!manager.isAutostartEnabled());

        manager.setAutostartEnabled(true);
        QVERIFY2(QFile::exists(autostartFile()), qPrintable(autostartFile()));
        QVERIFY(manager.isAutostartEnabled());

        QFile copied(autostartFile());
        QVERIFY(copied.open(QIODevice::ReadOnly));
        QVERIFY(copied.readAll().contains("Exec=/opt/crow/bin/crow"));
    }

    void testSwitchingItOffRemovesTheFile()
    {
        installedIn(QStringLiteral("opt/crow"), "[Desktop Entry]\nName=Crow Translate\n");

        UnixAutostartManager manager;
        manager.setAutostartEnabled(true);
        QVERIFY(manager.isAutostartEnabled());

        manager.setAutostartEnabled(false);
        QVERIFY(!QFile::exists(autostartFile()));
        QVERIFY(!manager.isAutostartEnabled());
    }

    // Switched on twice: the file that is there stays, and nothing is asked of
    // anybody.
    void testSwitchingItOnAgainLeavesTheFileAlone()
    {
        installedIn(QStringLiteral("opt/crow"), "[Desktop Entry]\nName=Crow Translate\n");

        UnixAutostartManager manager;
        manager.setAutostartEnabled(true);
        QFile edited(autostartFile());
        QVERIFY(edited.open(QIODevice::Append));
        edited.write("X-Edited-By-The-User=true\n");
        edited.close();

        manager.setAutostartEnabled(true);
        QFile kept(autostartFile());
        QVERIFY(kept.open(QIODevice::ReadOnly));
        QVERIFY(kept.readAll().contains("X-Edited-By-The-User=true"));
    }
};

QTEST_GUILESS_MAIN(AutostartTest)

#include "test_autostart.moc"
