/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

// Gotcha #14: TTS and translation state used to share a signal, so TTS
// autodetect could re-trigger translation detection in a loop (fixed
// 46d4e98b by splitting the signals and adding state guards). ATTSProvider
// and ATranslationProvider are now separate QObjects with disjoint signal
// sets - this test proves that structurally, by driving TTS play/pause/stop
// (backend None, deterministic, no hardware/network) through the real
// MainWindow and asserting the translator's languageDetected spy count never
// moves.
//
// Also covers a real, currently-unfixed gap flagged during this test-suite
// design: PiperTTSProvider::say() has no re-entrancy guard - a second call
// while a prior synthesis thread is still running starts a second detached
// QThread that races the first on the shared m_audioData member. This test
// may start RED; that's expected and intentional, same treatment as the
// project's existing QEXPECT_FAIL-marked known-broken test.

#include "mainwindow.h"
#include "singleapplication.h"
#include "sourcetextedit.h"
#include "testisolation.h"
#include "settings/appsettings.h"
#include "translator/atranslationprovider.h"
#include "tts/attsprovider.h"

#ifdef WITH_PIPER_TTS
#include "tts/piperttsprovider.h"
#endif

#include <QApplication>
#include <QKeyEvent>
#include <QSignalSpy>
#include <QTest>
#include <QToolButton>

// See the identical helper in test_translation.cpp: QTest::keyClicks()
// depends on host XKB keymap resolution, which isn't reliably available in
// sandboxed/nested test environments. Manually constructed QKeyEvents with
// explicit text() go through the same QApplication::sendEvent() delivery
// path without that dependency.
static void typeText(QWidget *widget, const QString &text)
{
    for (const QChar &ch : text) {
        QKeyEvent press(QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QString(ch));
        QKeyEvent release(QEvent::KeyRelease, Qt::Key_unknown, Qt::NoModifier, QString(ch));
        QApplication::sendEvent(widget, &press);
        QApplication::sendEvent(widget, &release);
    }
}

class TTSProviderIsolationTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        if (qgetenv("DISPLAY").isEmpty() && qgetenv("WAYLAND_DISPLAY").isEmpty())
            QSKIP("No display server available - skipping GUI tests");
    }

    void testTTSDrivingDoesNotTriggerTranslationDetection()
    {
        AppSettings settings;
        settings.setTTSProviderBackend(ATTSProvider::ProviderBackend::None);
        settings.setTranslationProviderBackend(ATranslationProvider::ProviderBackend::Copy);

        MainWindow window;
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));

        typeText(window.sourceEdit(), QStringLiteral("Hello"));

        auto *translator = window.findChild<ATranslationProvider *>();
        QVERIFY(translator != nullptr);
        QSignalSpy detectSpy(translator, &ATranslationProvider::languageDetected);

        QToolButton *playButton = window.sourcePlayPauseButton();
        QVERIFY(playButton != nullptr);

        QTest::mouseClick(playButton, Qt::LeftButton);
        QTest::qWait(300);
        QTest::mouseClick(playButton, Qt::LeftButton); // pause
        QTest::qWait(300);
        QTest::mouseClick(playButton, Qt::LeftButton); // resume/replay

        QTest::qWait(300);

        QCOMPARE(detectSpy.count(), 0);
    }

#ifdef WITH_PIPER_TTS
    void testPiperReentrantSayDoesNotCrash()
    {
        // No voice model configured in the test environment - both calls will
        // synthesize against empty state and finish with an error, which is
        // exactly what we want: any crash here comes from the missing
        // re-entrancy guard racing two threads on m_audioData, not from a
        // legitimately-absent model.
        PiperTTSProvider provider;

        QSignalSpy errorSpy(&provider, &ATTSProvider::errorOccurred);

        provider.say(QStringLiteral("first call"));
        provider.say(QStringLiteral("second call, while the first is still synthesizing"));

        QTest::qWait(1000);

        QVERIFY(true); // reaching here without crashing/UB is the assertion
    }
#endif
};

int main(int argc, char *argv[])
{
    isolateTestSettings();

    SingleApplication app(argc, argv, true);
    TTSProviderIsolationTest tc;
    QTEST_SET_MAIN_SOURCE_PATH
    return QTest::qExec(&tc, argc, argv);
}

#include "test_ttsprovider_isolation.moc"
