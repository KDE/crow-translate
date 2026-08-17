/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

// GUI-level integration tests for the module status system, driven through a
// real MainWindow on the Copy backend (fully synchronous, no network):
// what moduleStatus() reports while a translation cascades through
// MainWindow's own stateChanged/translationAccepted/resetTranslator wiring,
// what the pop-up's strip does with it, and the Interface/ShowStatusBar
// pick-up in loadAppSettings().
//
// Copy's translate() is synchronous, so its Busy interval only exists during
// the emission cascade - observing it requires recording snapshots from a
// changed() handler connected to the model (a direct connection fires inside
// the cascade), not sampling after the click returns.

#include "language.h"
#include "mainwindow.h"
#include "modulestatus.h"
#include "popupwindow.h"
#include "singleapplication.h"
#include "sourcetextedit.h"
#include "statusstrip.h"
#include "testisolation.h"
#include "settings/appsettings.h"
#include "translator/atranslationprovider.h"
#include "tts/attsprovider.h"

#include <QApplication>
#include <QKeyEvent>
#include <QLocale>
#include <QStatusBar>
#include <QTest>

#include <algorithm>
#include <vector>

class MainWindowStatusTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();

    void testModelExposedAndTtsUnavailableWithNone();
    void testBusyObservedDuringTranslateClick();
    void testStickyErrorThroughRealCascade();
    void testStickyErrorClearedByNextTranslation();
    void testPopupStripOnlyVisibleWhileBusy();
    void testShowStatusBarSettingPickUp();

private:
    static void pinCopySettings();
};

void MainWindowStatusTest::initTestCase()
{
    if (qgetenv("DISPLAY").isEmpty() && qgetenv("WAYLAND_DISPLAY").isEmpty()) {
        QSKIP("No display server available - skipping GUI tests");
    }

    Q_INIT_RESOURCE(engines);
    Q_INIT_RESOURCE(icon_theme);

    pinCopySettings();
}

void MainWindowStatusTest::cleanup()
{
    // Backends and languages drift per test; re-pin for the next one.
    pinCopySettings();
}

void MainWindowStatusTest::pinCopySettings()
{
    AppSettings settings;
    settings.setShowPrivacyPopup(false);
    settings.setTranslationProviderBackend(ATranslationProvider::ProviderBackend::Copy);
    settings.setTTSProviderBackend(ATTSProvider::ProviderBackend::None);
    settings.setWindowMode(AppSettings::MainWindow);

    const Language systemLanguage(QLocale::system());
    settings.setLanguages(AppSettings::Source, {systemLanguage});
    settings.setLanguages(AppSettings::Translation, {systemLanguage});
    settings.setCheckedButton(AppSettings::Source, 0);
    settings.setCheckedButton(AppSettings::Translation, 0);
}

void MainWindowStatusTest::testModelExposedAndTtsUnavailableWithNone()
{
    MainWindow window;
    QVERIFY(window.moduleStatus() != nullptr);
    QVERIFY(!window.moduleStatus()->isBusy());

    // TTS backend None: NoopTTSProvider never emits and the strip omits the
    // segment entirely.
    QVERIFY(!window.moduleStatus()->isAvailable(ModuleStatus::Module::Tts));

    // The strip itself lives in the main window's status bar.
    QStatusBar *bar = window.findChild<QStatusBar *>(QStringLiteral("statusbar"));
    QVERIFY(bar != nullptr);
    QVERIFY(bar->findChild<StatusStrip *>() != nullptr);
}

void MainWindowStatusTest::testBusyObservedDuringTranslateClick()
{
    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    std::vector<ModuleStatus::Activity> seen;
    std::vector<QString> messages;
    connect(window.moduleStatus(), &ModuleStatus::changed, [&]() {
        seen.push_back(window.moduleStatus()->activity(ModuleStatus::Module::Translation));
        messages.push_back(window.moduleStatus()->message(ModuleStatus::Module::Translation));
    });

    // Same typeText-then-click flow as test_translation.cpp's
    // testGuiTranslation; sourceEdit's textEdited is debounced, so wait for
    // the translate button to arm first.
    for (const QChar &ch : QStringLiteral("Hello World")) {
        QKeyEvent press(QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QString(ch));
        QKeyEvent release(QEvent::KeyRelease, Qt::Key_unknown, Qt::NoModifier, QString(ch));
        QApplication::sendEvent(window.sourceEdit(), &press);
        QApplication::sendEvent(window.sourceEdit(), &release);
    }
    QVERIFY(QTest::qWaitFor([&window]() {
        return window.translateButton()->isEnabled();
    },
                            3000));
    QTest::mouseClick(window.translateButton(), Qt::LeftButton);

    QVERIFY(window.translationEdit()->toPlainText() == QStringLiteral("Hello World"));
    QVERIFY(std::find(seen.begin(), seen.end(), ModuleStatus::Activity::Busy) != seen.end());
    QVERIFY(std::find(messages.begin(), messages.end(), QStringLiteral("Translating")) != messages.end());
    QCOMPARE(window.moduleStatus()->activity(ModuleStatus::Module::Translation), ModuleStatus::Activity::Idle);
    QVERIFY(!window.moduleStatus()->isBusy());
}

// A Finished-with-error runs the full synchronous cascade -
// translatorStateChanged() writes the error into translationEdit and emits
// resetTranslator(), whose reset() lands on Ready in the same call stack -
// and the model must still report the error afterwards. This is the
// integration twin of test_modulestatus's stickiness test, through
// MainWindow's real wiring instead of a bare provider.
void MainWindowStatusTest::testStickyErrorThroughRealCascade()
{
    MainWindow window;

    // translationRequested is a public signal in Qt 6; emitting it drives
    // MainWindow::handleTranslationRequest exactly like the translate button
    // does, but with a language pair the Copy backend rejects.
    emit window.translationRequested(QStringLiteral("hello"),
                                     Language(QLocale::French),
                                     Language(QLocale::English));

    QCOMPARE(window.moduleStatus()->activity(ModuleStatus::Module::Translation), ModuleStatus::Activity::Error);
    QVERIFY(!window.moduleStatus()->detail(ModuleStatus::Module::Translation).isEmpty());
    QVERIFY(window.translationEdit()->toPlainText().contains(QStringLiteral("Error")));
}

void MainWindowStatusTest::testStickyErrorClearedByNextTranslation()
{
    MainWindow window;

    emit window.translationRequested(QStringLiteral("hello"),
                                     Language(QLocale::French),
                                     Language(QLocale::English));
    QCOMPARE(window.moduleStatus()->activity(ModuleStatus::Module::Translation), ModuleStatus::Activity::Error);

    // A matching pair reaches Processed/NoError; the cascade ends Idle and
    // the sticky error is gone.
    emit window.translationRequested(QStringLiteral("hello"),
                                     Language(QLocale::English),
                                     Language(QLocale::English));
    QCOMPARE(window.moduleStatus()->activity(ModuleStatus::Module::Translation), ModuleStatus::Activity::Idle);
    QVERIFY(window.translationEdit()->toPlainText() == QStringLiteral("hello"));
}

// The pop-up window's strip mirrors the main window's model and only appears
// while something is running, so the resting pop-up is unchanged.
void MainWindowStatusTest::testPopupStripOnlyVisibleWhileBusy()
{
    MainWindow window;
    window.hide();

    PopupWindow popup(&window);
    StatusStrip *strip = popup.findChild<StatusStrip *>(QStringLiteral("statusStrip"));
    QVERIFY(strip != nullptr);
    QVERIFY(strip->isHidden()); // at rest

    // beginScreenCapture() is a public slot on the model; it is the one
    // moment with no provider signal to hang off.
    window.moduleStatus()->beginScreenCapture();
    QVERIFY(!strip->isHidden());
}

// Interface/ShowStatusBar pick-up happens in loadAppSettings(), which re-runs
// after the settings dialog is accepted - each new window reads it fresh.
void MainWindowStatusTest::testShowStatusBarSettingPickUp()
{
    AppSettings settings;

    settings.setShowStatusBar(false);
    {
        MainWindow window;
        QStatusBar *bar = window.findChild<QStatusBar *>(QStringLiteral("statusbar"));
        QVERIFY(bar != nullptr);
        QVERIFY(bar->isHidden());

        // The setting hides the whole bar; the strip inside goes with it and
        // nothing the model does can bring it back.
        window.moduleStatus()->beginScreenCapture();
        QVERIFY(bar->isHidden());
    }

    settings.setShowStatusBar(true);
    {
        MainWindow window;
        QStatusBar *bar = window.findChild<QStatusBar *>(QStringLiteral("statusbar"));
        QVERIFY(bar != nullptr);
        QVERIFY(!bar->isHidden());
    }
}

int main(int argc, char *argv[])
{
    isolateTestSettings();

    SingleApplication app(argc, argv, true);
    MainWindowStatusTest tc;
    QTEST_SET_MAIN_SOURCE_PATH
    return QTest::qExec(&tc, argc, argv);
}

#include "test_mainwindow_status.moc"
