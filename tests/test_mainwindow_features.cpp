/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

// GUI-level feature tests through a real MainWindow: swap, clear, the copy
// buttons, abort, the paste-image OCR chain, Escape-cancels-image, and the
// Interface/ShowStatusBar settings round-trip. Everything runs offline - the
// LocalAI translation backend and the LLM OCR engine are pointed at
// MockHttpServer on 127.0.0.1 via the same settings keys the settings dialog
// writes, so these exercise the full MainWindow -> provider -> UI path with
// deterministic canned responses (and hang=true for the abort cases, the
// only way to hold Processing/recognizing open long enough to interact).

#include "language.h"
#include "languagebuttonswidget.h"
#include "mainwindow.h"
#include "mockhttpserver.h"
#include "modulestatus.h"
#include "singleapplication.h"
#include "sourcetextedit.h"
#include "testisolation.h"
#include "ocr/llmocr.h"
#include "settings/appsettings.h"
#include "settings/settingsdialog.h"
#include "translator/atranslationprovider.h"
#include "translator/localaitranslationprovider.h"
#include "tts/attsprovider.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QKeyEvent>
#include <QLocale>
#include <QTest>
#include <QTextEdit>
#include <QToolButton>

#include <algorithm>

namespace
{
QByteArray chatCompletionJson(const QString &content)
{
    return QStringLiteral(R"({"choices":[{"message":{"role":"assistant","content":"%1"}}]})").arg(content).toUtf8();
}

MockHttpServer::Response hangResponse()
{
    MockHttpServer::Response response;
    response.hang = true;
    return response;
}

// Drives the LocalAI backend (Translation/Backend=2) at the mock server. The
// provider id must be one of AppSettings::localProviderIds() so the engine
// combo population in updateProviderUI() stays consistent.
void pinLocalAiSettings(const MockHttpServer &server)
{
    AppSettings settings;
    settings.setShowPrivacyPopup(false);
    settings.setTranslationProviderBackend(ATranslationProvider::ProviderBackend::LocalAI);
    settings.setTTSProviderBackend(ATTSProvider::ProviderBackend::None);
    settings.setActiveLocalProvider(QStringLiteral("openai_custom"));
    settings.setLocalProviderUrl(QStringLiteral("openai_custom"), server.baseUrl());
    settings.setLocalProviderModel(QStringLiteral("openai_custom"), QStringLiteral("mock-model"));
    settings.setLocalAiTimeout(QStringLiteral("openai_custom"), 300);
    settings.setForceSourceAutodetect(false);
    settings.setForceTranslationAutodetect(false);

    const Language english(QLocale::English);
    const Language spanish(QLocale::Spanish);
    settings.setLanguages(AppSettings::Source, {english});
    settings.setLanguages(AppSettings::Translation, {spanish});
    settings.setCheckedButton(AppSettings::Source, 0);
    settings.setCheckedButton(AppSettings::Translation, 0);
}

void pinLlmOcrSettings(const MockHttpServer &server)
{
    AppSettings settings;
    settings.setOcrEngine(AppSettings::OcrEngine::Llm);
    settings.setOcrLlmProvider(QStringLiteral("openai_custom"));
    settings.setOcrLlmUrl(QStringLiteral("openai_custom"), server.baseUrl());
    settings.setOcrLlmModel(QStringLiteral("openai_custom"), QStringLiteral("mock-model"));
}

// Same delivery rationale as test_translation.cpp's typeText(): synthesized
// platform key events can't inject into this window on Wayland, manually
// constructed QKeyEvents through sendEvent() exercise the real handlers.
void sendKey(QWidget *widget, int key, const QString &text = {})
{
    QKeyEvent press(QEvent::KeyPress, key, Qt::NoModifier, text);
    QKeyEvent release(QEvent::KeyRelease, key, Qt::NoModifier, text);
    QApplication::sendEvent(widget, &press);
    QApplication::sendEvent(widget, &release);
}

void typeText(QWidget *widget, const QString &text)
{
    for (const QChar &ch : text)
        sendKey(widget, Qt::Key_unknown, QString(ch));
}

QImage solidImage()
{
    QImage image(64, 32, QImage::Format_ARGB32);
    image.fill(Qt::white);
    return image;
}
} // namespace

class MainWindowFeaturesTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void cleanup();

    void testSwapSwapsTextsAndLanguages();
    void testClearButtonClearsBothEdits();
    void testCopyButtons();
    void testAbortReturnsToReadyWithoutError();
    void testPasteImageRecognizeAndAutoTranslate();
    void testEscapeCancelsSourceImage();
    void testShowStatusBarSettingsRoundTrip();
    void testRecognizedTextActivatesUiWithoutKeystroke();
    void testRecognizedTextAutoTranslatesWhenEnabled();

private:
    static void waitForTranslateButton(MainWindow &window);
};

void MainWindowFeaturesTest::initTestCase()
{
    if (qgetenv("DISPLAY").isEmpty() && qgetenv("WAYLAND_DISPLAY").isEmpty()) {
        QSKIP("No display server available - skipping GUI tests");
    }

    Q_INIT_RESOURCE(engines);
    Q_INIT_RESOURCE(icon_theme);
}

void MainWindowFeaturesTest::cleanupTestCase()
{
    // Leave the shared isolated settings in the Copy/None state the rest of
    // the suite expects, whatever the last test happened to pin.
    AppSettings settings;
    settings.setTranslationProviderBackend(ATranslationProvider::ProviderBackend::Copy);
    settings.setTTSProviderBackend(ATTSProvider::ProviderBackend::None);
    settings.setOcrEngine(AppSettings::OcrEngine::Tesseract);
    QApplication::clipboard()->clear();
}

void MainWindowFeaturesTest::cleanup()
{
    QApplication::clipboard()->clear();
}

void MainWindowFeaturesTest::waitForTranslateButton(MainWindow &window)
{
    QVERIFY(QTest::qWaitFor([&window]() {
        return window.translateButton()->isEnabled();
    },
                            3000));
}

// Swap must exchange the edits' contents AND the language buttons' checked
// languages - the translation direction has to flip with the text.
void MainWindowFeaturesTest::testSwapSwapsTextsAndLanguages()
{
    MockHttpServer server;
    server.queueJson(200, chatCompletionJson(QStringLiteral("mocked output")));
    pinLocalAiSettings(server);
    pinLlmOcrSettings(server); // keep prepareOcr() usable regardless of engine

    MainWindow window;
    typeText(window.sourceEdit(), QStringLiteral("Hello"));
    waitForTranslateButton(window);
    QTest::mouseClick(window.translateButton(), Qt::LeftButton);

    QVERIFY(QTest::qWaitFor([&window]() {
        return window.translationEdit()->toPlainText() == QStringLiteral("mocked output");
    },
                            5000));

    const Language sourceBefore = window.sourceLanguageButtons()->checkedLanguage();
    const Language destBefore = window.translationLanguageButtons()->checkedLanguage();

    QTest::mouseClick(window.swapButton(), Qt::LeftButton);

    // The texts exchange places (the old source text follows to the
    // translation side), and the checked languages flip with them.
    QCOMPARE(window.sourceEdit()->toPlainText(), QStringLiteral("mocked output"));
    QCOMPARE(window.translationEdit()->toPlainText(), QStringLiteral("Hello"));
    QCOMPARE(window.sourceLanguageButtons()->checkedLanguage(), destBefore);
    QCOMPARE(window.translationLanguageButtons()->checkedLanguage(), sourceBefore);
}

void MainWindowFeaturesTest::testClearButtonClearsBothEdits()
{
    MockHttpServer server;
    server.queueJson(200, chatCompletionJson(QStringLiteral("mocked output")));
    pinLocalAiSettings(server);

    MainWindow window;
    typeText(window.sourceEdit(), QStringLiteral("Hello"));
    waitForTranslateButton(window);
    QTest::mouseClick(window.translateButton(), Qt::LeftButton);
    QVERIFY(QTest::qWaitFor([&window]() {
        return !window.translationEdit()->toPlainText().isEmpty();
    },
                            5000));

    QToolButton *clearButton = window.findChild<QToolButton *>(QStringLiteral("clearButton"));
    QVERIFY(clearButton != nullptr);
    QTest::mouseClick(clearButton, Qt::LeftButton);

    QVERIFY(window.sourceEdit()->toPlainText().isEmpty());
    QVERIFY(window.translationEdit()->toPlainText().isEmpty());
}

void MainWindowFeaturesTest::testCopyButtons()
{
    MockHttpServer server;
    server.queueJson(200, chatCompletionJson(QStringLiteral("mocked output")));
    pinLocalAiSettings(server);

    MainWindow window;
    typeText(window.sourceEdit(), QStringLiteral("Hello"));
    waitForTranslateButton(window);
    QTest::mouseClick(window.translateButton(), Qt::LeftButton);
    QVERIFY(QTest::qWaitFor([&window]() {
        return !window.translationEdit()->toPlainText().isEmpty();
    },
                            5000));

    QTest::mouseClick(window.copySourceButton(), Qt::LeftButton);
    QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("Hello"));

    QTest::mouseClick(window.copyTranslationButton(), Qt::LeftButton);
    QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("mocked output"));

    // "Copy all" joins source and translation with a newline.
    QTest::mouseClick(window.copyAllTranslationButton(), Qt::LeftButton);
    QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("Hello\nmocked output"));
}

// Aborting a hung translation must land back at Ready with the strip Idle -
// not Error: a deliberate abort is the user's own action, not a failure.
void MainWindowFeaturesTest::testAbortReturnsToReadyWithoutError()
{
    MockHttpServer server;
    server.queueResponse(hangResponse());
    pinLocalAiSettings(server);

    MainWindow window;
    typeText(window.sourceEdit(), QStringLiteral("Hello"));
    waitForTranslateButton(window);

    bool sawBusy = false;
    connect(window.moduleStatus(), &ModuleStatus::changed, [&]() {
        sawBusy = sawBusy || window.moduleStatus()->activity(ModuleStatus::Module::Translation) == ModuleStatus::Activity::Busy;
    });

    QTest::mouseClick(window.translateButton(), Qt::LeftButton);
    QVERIFY(QTest::qWaitFor([&window]() {
        return window.findChild<QToolButton *>(QStringLiteral("abortButton"))->isEnabled();
    },
                            5000));
    QVERIFY(sawBusy);
    QCOMPARE(window.moduleStatus()->activity(ModuleStatus::Module::Translation), ModuleStatus::Activity::Busy);

    QTest::mouseClick(window.findChild<QToolButton *>(QStringLiteral("abortButton")), Qt::LeftButton);

    QVERIFY(QTest::qWaitFor([&window]() {
        return window.moduleStatus()->activity(ModuleStatus::Module::Translation) == ModuleStatus::Activity::Idle;
    },
                            5000));
    QCOMPARE(window.moduleStatus()->activity(ModuleStatus::Module::Translation), ModuleStatus::Activity::Idle);

    // Existing behavior, deliberately pinned: MainWindow writes the abort
    // error text into translationEdit (translatorStateChanged's Finished
    // branch) even though the strip correctly reports Idle, not Error.
    QVERIFY(window.translationEdit()->toPlainText().contains(QStringLiteral("Error")));
}

// Paste an image into sourceEdit: recognition starts immediately (dropped
// image = transcribe it), the recognized text lands in sourceEdit, and with
// auto-translate on the translation follows - the full offline chain.
void MainWindowFeaturesTest::testPasteImageRecognizeAndAutoTranslate()
{
    MockHttpServer server;
    server.queueJson(200, chatCompletionJson(QStringLiteral("HELLO FROM IMAGE")));
    server.queueJson(200, chatCompletionJson(QStringLiteral("mocked translation")));
    pinLocalAiSettings(server);
    pinLlmOcrSettings(server);

    AppSettings settings;
    settings.setAutoTranslateEnabled(true);

    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    bool sawOcrBusy = false;
    connect(window.moduleStatus(), &ModuleStatus::changed, [&]() {
        sawOcrBusy = sawOcrBusy || window.moduleStatus()->activity(ModuleStatus::Module::Ocr) == ModuleStatus::Activity::Busy;
    });

    QApplication::clipboard()->setImage(solidImage());
    // The eventFilter handles Ctrl+V for images: matches(QKeySequence::Paste)
    // needs the Control modifier on the synthesized event.
    QKeyEvent paste(QEvent::KeyPress, Qt::Key_V, Qt::ControlModifier);
    QApplication::sendEvent(window.sourceEdit(), &paste);

    QVERIFY(QTest::qWaitFor([&window]() {
        return window.sourceEdit()->toPlainText() == QStringLiteral("HELLO FROM IMAGE");
    },
                            8000));
    QVERIFY(sawOcrBusy);

    QVERIFY(QTest::qWaitFor([&window]() {
        return window.translationEdit()->toPlainText() == QStringLiteral("mocked translation");
    },
                            8000));
}

// Escape while recognition is still running cancels it: preview cleared,
// language buttons re-enabled, strip back to Idle.
void MainWindowFeaturesTest::testEscapeCancelsSourceImage()
{
    MockHttpServer server;
    server.queueResponse(hangResponse()); // recognition hangs until cancelled
    pinLocalAiSettings(server);
    pinLlmOcrSettings(server);

    MainWindow window;

    QApplication::clipboard()->setImage(solidImage());
    QKeyEvent paste(QEvent::KeyPress, Qt::Key_V, Qt::ControlModifier);
    QApplication::sendEvent(window.sourceEdit(), &paste);

    QVERIFY(QTest::qWaitFor([&window]() {
        return window.moduleStatus()->activity(ModuleStatus::Module::Ocr) == ModuleStatus::Activity::Busy;
    },
                            5000));
    QVERIFY(!window.sourceLanguageButtons()->isEnabled());

    sendKey(&window, Qt::Key_Escape);

    QVERIFY(QTest::qWaitFor([&window]() {
        return window.moduleStatus()->activity(ModuleStatus::Module::Ocr) == ModuleStatus::Activity::Idle;
    },
                            5000));
    QVERIFY(window.sourceEdit()->toPlainText().isEmpty());
    QVERIFY(window.sourceLanguageButtons()->isEnabled());
}

// SettingsDialog accept() must persist the checkbox, restoreDefaults() must
// reset it - the four-point pattern minus the two UI-only touch points.
void MainWindowFeaturesTest::testShowStatusBarSettingsRoundTrip()
{
    MainWindow window;

    {
        SettingsDialog dialog(&window);
        QCheckBox *box = dialog.findChild<QCheckBox *>(QStringLiteral("showStatusBarCheckBox"));
        QVERIFY(box != nullptr);
        QVERIFY(box->isChecked()); // default true

        box->setChecked(false);
        dialog.accept();
    }
    QCOMPARE(AppSettings().isShowStatusBar(), false);

    {
        SettingsDialog dialog(&window);
        QCheckBox *box = dialog.findChild<QCheckBox *>(QStringLiteral("showStatusBarCheckBox"));
        QVERIFY(box != nullptr);
        QVERIFY(!box->isChecked());

        // Private slot - reachable through the meta-object by name.
        QVERIFY(QMetaObject::invokeMethod(&dialog, "restoreDefaults", Qt::DirectConnection));
        QVERIFY(box->isChecked());
        dialog.accept();
    }
    QCOMPARE(AppSettings().isShowStatusBar(), true);
}

// The user-visible regression this file guards: text inserted by OCR used to
// leave the translate button disabled until the user typed something (a
// space) - SourceTextEdit::replaceText() deliberately suppresses textEdited,
// and textEdited is what drives every follow-up typed text gets. The
// recognized handler must run them itself, with no keystroke involved.
void MainWindowFeaturesTest::testRecognizedTextActivatesUiWithoutKeystroke()
{
    MockHttpServer server;
    server.queueJson(200, chatCompletionJson(QStringLiteral("en"))); // detection response
    pinLocalAiSettings(server);
    pinLlmOcrSettings(server);
    AppSettings settings;
    settings.setDetectProvider(QStringLiteral("openai_custom"));
    settings.setDetectModel(QStringLiteral("mock-model"));

    MainWindow window;
    QCheckBox *autoTranslate = window.findChild<QCheckBox *>(QStringLiteral("autoTranslateCheckBox"));
    QVERIFY(autoTranslate != nullptr);
    autoTranslate->setChecked(false); // pin: a prior test may have left it on

    QVERIFY(!window.translateButton()->isEnabled()); // empty source: disabled

    bool sawDetecting = false;
    connect(window.moduleStatus(), &ModuleStatus::changed, [&]() {
        sawDetecting = sawDetecting || window.moduleStatus()->message(ModuleStatus::Module::Translation) == QStringLiteral("Detecting language");
    });

    // Not a keystroke - the real engine signal, exactly as recognize()
    // delivers it. With auto-translate off, the follow-up is standalone
    // language detection (updateAutoLocales), which is async against the
    // mock and must show in the strip while in flight.
    emit window.ocr()->recognized(QStringLiteral("Hello from OCR"));

    QCOMPARE(window.sourceEdit()->toPlainText(), QStringLiteral("Hello from OCR"));
    QVERIFY(window.translateButton()->isEnabled());
    QVERIFY(QTest::qWaitFor([&window]() {
        return window.moduleStatus()->activity(ModuleStatus::Module::Translation) == ModuleStatus::Activity::Idle;
    },
                            5000));
    QVERIFY(sawDetecting);
}

// Same path with auto-translate on: the recognized text must chain straight
// into a translation, like typed text does - no keystroke, no button click.
void MainWindowFeaturesTest::testRecognizedTextAutoTranslatesWhenEnabled()
{
    MockHttpServer server;
    server.queueJson(200, chatCompletionJson(QStringLiteral("mocked translation")));
    pinLocalAiSettings(server);
    pinLlmOcrSettings(server);

    MainWindow window;
    QCheckBox *autoTranslate = window.findChild<QCheckBox *>(QStringLiteral("autoTranslateCheckBox"));
    QVERIFY(autoTranslate != nullptr);
    autoTranslate->setChecked(true);

    emit window.ocr()->recognized(QStringLiteral("Hello from OCR"));

    QVERIFY(QTest::qWaitFor([&window]() {
        return window.translationEdit()->toPlainText() == QStringLiteral("mocked translation");
    },
                            5000));
    QVERIFY(window.translateButton()->isEnabled());
}

int main(int argc, char *argv[])
{
    isolateTestSettings();

    SingleApplication app(argc, argv, true);
    MainWindowFeaturesTest tc;
    QTEST_SET_MAIN_SOURCE_PATH
    return QTest::qExec(&tc, argc, argv);
}

#include "test_mainwindow_features.moc"
