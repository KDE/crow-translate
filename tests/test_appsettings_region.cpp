/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

// "Remember the region" for screen recognition, set to "always", keeps the
// last selected region in the settings. Choosing anything else is meant to
// forget it - and removed a key the region was never stored under, so it
// stayed, and came back the next time "always" was chosen.

#include "testisolation.h"
#include "settings/appsettings.h"

#include <QCoreApplication>
#include <QRect>
#include <QTest>

class AppSettingsRegionTest : public QObject
{
    Q_OBJECT

private slots:
    void testNoRegionIsRememberedToBeginWith()
    {
        QVERIFY(AppSettings().cropRegion().isNull());
    }

    void testARegionRememberedAlwaysIsKept()
    {
        AppSettings settings;
        settings.setRegionRememberType(AppSettings::RememberAlways);
        settings.setCropRegion(QRect(10, 20, 300, 200));
        QCOMPARE(AppSettings().cropRegion(), QRect(10, 20, 300, 200));

        // Saving the same choice again - the settings dialog does on every OK -
        // does not lose it.
        settings.setRegionRememberType(AppSettings::RememberAlways);
        QCOMPARE(AppSettings().cropRegion(), QRect(10, 20, 300, 200));
    }

    void testChoosingNotToRememberForgetsTheRegion_data()
    {
        QTest::addColumn<AppSettings::RegionRememberType>("choice");
        QTest::newRow("never") << AppSettings::NeverRemember;
        QTest::newRow("until exit") << AppSettings::RememberLast;
    }

    void testChoosingNotToRememberForgetsTheRegion()
    {
        QFETCH(AppSettings::RegionRememberType, choice);

        AppSettings settings;
        settings.setRegionRememberType(AppSettings::RememberAlways);
        settings.setCropRegion(QRect(10, 20, 300, 200));

        settings.setRegionRememberType(choice);
        QVERIFY2(AppSettings().cropRegion().isNull(), "the region is still in the settings");

        // ... so that it is not what "always" starts from the next time.
        settings.setRegionRememberType(AppSettings::RememberAlways);
        QVERIFY(AppSettings().cropRegion().isNull());
    }
};

int main(int argc, char *argv[])
{
    isolateTestSettings();
    QCoreApplication app(argc, argv);
    AppSettingsRegionTest tc;
    QTEST_SET_MAIN_SOURCE_PATH
    return QTest::qExec(&tc, argc, argv);
}

#include "test_appsettings_region.moc"
