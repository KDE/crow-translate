/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

// Which frontend an invocation selects.
//
// resolveId() runs before any QCoreApplication exists - the answer decides
// which one to build - so it cannot use QCommandLineParser and is checked
// here against the argument lists it will really see. Headless: it is string
// handling, and nothing is constructed.

#include "frontend/frontendregistry.h"

#include <QTest>

class FrontendRegistryTest : public QObject
{
    Q_OBJECT

private slots:
    void testResolveId_data()
    {
        QTest::addColumn<QStringList>("arguments");
        QTest::addColumn<QString>("expected");

        // The rule main() used to implement by testing argc == 1.
        QTest::newRow("no arguments is the window") << QStringList{QStringLiteral("crow")} << QStringLiteral("gui");
        QTest::newRow("any argument is the command line") << QStringList{QStringLiteral("crow"), QStringLiteral("hello")} << QStringLiteral("cli");

        // What the old rule could not express: the window, with arguments.
        QTest::newRow("explicit gui") << QStringList{QStringLiteral("crow"), QStringLiteral("--frontend"), QStringLiteral("gui")} << QStringLiteral("gui");
        QTest::newRow("explicit gui, equals form") << QStringList{QStringLiteral("crow"), QStringLiteral("--frontend=gui")} << QStringLiteral("gui");
        QTest::newRow("explicit gui alongside other options")
            << QStringList{QStringLiteral("crow"), QStringLiteral("-s"), QStringLiteral("en"), QStringLiteral("--frontend"), QStringLiteral("gui")}
            << QStringLiteral("gui");

        QTest::newRow("explicit cli with no other arguments")
            << QStringList{QStringLiteral("crow"), QStringLiteral("--frontend"), QStringLiteral("cli")} << QStringLiteral("cli");

        // Reported by main(), which can list what does exist. Deciding that
        // here would mean guessing on the user's behalf.
        QTest::newRow("unknown name is passed through")
            << QStringList{QStringLiteral("crow"), QStringLiteral("--frontend"), QStringLiteral("nonsense")} << QStringLiteral("nonsense");

        // A value-less --frontend goes to the command line, whose parser
        // produces a proper "requires a value" message.
        QTest::newRow("missing value falls back to the command line") << QStringList{QStringLiteral("crow"), QStringLiteral("--frontend")} << QStringLiteral("cli");
    }

    void testResolveId()
    {
        QFETCH(QStringList, arguments);
        QFETCH(QString, expected);
        QCOMPARE(FrontendRegistry::resolveId(arguments), expected);
    }

    void testEveryAdvertisedFrontendCanBeCreated()
    {
        const QList<FrontendInfo> frontends = FrontendRegistry::available();
        QVERIFY(!frontends.isEmpty());

        for (const FrontendInfo &info : frontends) {
            QVERIFY2(!info.displayName.isEmpty(), qPrintable(QStringLiteral("'%1' has no display name").arg(info.id)));
            QVERIFY2(FrontendRegistry::find(info.id).has_value(), qPrintable(QStringLiteral("'%1' is listed but cannot be found").arg(info.id)));
            // Construction must not need an application object - main() does
            // this before deciding which one to build.
            QVERIFY2(FrontendRegistry::create(info.id) != nullptr, qPrintable(QStringLiteral("'%1' is listed but cannot be created").arg(info.id)));
        }
    }

    void testUnknownFrontendIsNotCreated()
    {
        QVERIFY(!FrontendRegistry::find(QStringLiteral("nonsense")).has_value());
        QVERIFY(FrontendRegistry::create(QStringLiteral("nonsense")) == nullptr);
    }
};

QTEST_GUILESS_MAIN(FrontendRegistryTest)

#include "test_frontendregistry.moc"
