/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

// Regression guard for a real bug: file-scope/static QDBusInterface objects
// get constructed before main() runs, which historically corrupted Qt's
// internal D-Bus state (QTBUG-135928-style) - service registration would
// succeed but method calls would silently time out. Fixed by converting
// every QDBusInterface to an instance member (see waylandgnomescreengrabber.cpp,
// waylandplasmascreengrabber.cpp, waylandportalscreengrabber.cpp,
// portalautostartmanager.cpp). This test asserts that stays true: no
// file-scope or class-static QDBusInterface declaration anywhere under src/.
//
// Pure source-text scan, no display/QApplication/D-Bus session needed -
// runs fine headless.

#include <QDir>
#include <QDirIterator>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QTest>

class DBusStaticInitTest : public QObject
{
    Q_OBJECT

private slots:
    void testNoStaticQDBusInterface()
    {
        // Matches "static ... QDBusInterface" at the start of a (trimmed) line,
        // which is how a file-scope or class-static declaration reads in this
        // codebase's style. Deliberately does not try to parse C++ properly -
        // this is a regression tripwire, not a linter.
        static const QRegularExpression staticDeclaration(QStringLiteral("^static\\s+.*QDBusInterface"));

        QStringList offenders;

        QDirIterator it(QStringLiteral(SRC_DIR), {QStringLiteral("*.h"), QStringLiteral("*.cpp")}, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString filePath = it.next();
            if (filePath.contains(QStringLiteral("/3rdparty/")))
                continue;

            QFile file(filePath);
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
                continue;

            int lineNumber = 0;
            while (!file.atEnd()) {
                ++lineNumber;
                const QString line = QString::fromUtf8(file.readLine()).trimmed();
                if (staticDeclaration.match(line).hasMatch())
                    offenders << QStringLiteral("%1:%2: %3").arg(filePath).arg(lineNumber).arg(line);
            }
        }

        QVERIFY2(offenders.isEmpty(), qUtf8Printable(QStringLiteral("Found static QDBusInterface declaration(s), which can corrupt Qt's D-Bus state before main() runs:\n") + offenders.join('\n')));
    }
};

QTEST_APPLESS_MAIN(DBusStaticInitTest)
#include "test_dbus_static_init.moc"
