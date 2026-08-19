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
#include "popupwindow.h"
#include "singleapplication.h"
#include "sourcetextedit.h"
#include "testisolation.h"
#include "ocr/llmocr.h"
#include "settings/appsettings.h"
#include "settings/settingsdialog.h"
#include "translator/atranslationprovider.h"
#include "translator/localaitranslationprovider.h"
#include "tts/attsprovider.h"
#include "tts/voice.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
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
    void testForcedAutoSpeaksTheResolvedDestinationLanguage();
    void testVoiceComboOffersTheResolvedLanguagesVoices();
    void testPiperComboOffersTheResolvedLanguagesVoices();
    void testAutoDestinationStaysAutoAtStartup();
    void testRetranslateDoesNotClobberNextAutoDestination();
    void testPopupShowsTheTtsControlsTheProviderAskedFor();
    void testPopupHidesTtsControlsWhenThereIsNoTtsProvider();
    void testPopupEngineComboFollowsTheActiveBackend();
    void testPopupCombosKeepTrackingTheMainWindowAfterItIsOpen();
    void testPopupVoicesMatchTheMainWindowAfterARealTranslation();
    void testAutoButtonNamesTheDestinationItResolvedTo();

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
    settings.setShowTrayIcon(false);
    settings.setStartMinimized(false);
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

// The reported regression, in the shape the primary->secondary rule creates:
// source is auto, destination is auto, and the detected source EQUALS the
// primary language - so auto resolves to the SECONDARY. Translation goes to
// Russian correctly, but speech was read in the system locale (English),
// because with auto checked m_destLang stays auto - the retranslate handler
// only assigns it when a retranslation happens to be needed, which it is not
// when the first request already picked the right destination.
void MainWindowFeaturesTest::testForcedAutoSpeaksTheResolvedDestinationLanguage()
{
    MockHttpServer server;
    // Several, since settings leak between tests in this fixture and a
    // left-over auto-translate would otherwise eat the only queued reply.
    for (int i = 0; i < 4; ++i) {
        server.queueJson(200, chatCompletionJson(QStringLiteral("privet")));
    }
    pinLocalAiSettings(server);

    const Language english(QLocale::English);
    const Language russian(QLocale::Russian);

    AppSettings settings;
    settings.setAutoTranslateEnabled(false);
    // Source == primary, so the destination must fall through to secondary.
    settings.setPrimaryLanguage(english);
    settings.setSecondaryLanguage(russian);
    settings.setLanguages(AppSettings::Source, {english});
    settings.setLanguages(AppSettings::Translation, {english});

    MainWindow window;
    // What force-autodetect does before a scripted translation.
    window.sourceLanguageButtons()->checkAutoButton();
    window.translationLanguageButtons()->checkAutoButton();

    typeText(window.sourceEdit(), QStringLiteral("Hello"));
    waitForTranslateButton(window);
    QTest::mouseClick(window.translateButton(), Qt::LeftButton);

    QVERIFY(QTest::qWaitFor([&window]() {
        return window.translationEdit()->toPlainText() == QStringLiteral("privet");
    },
                            5000));

    // Sanity: the system locale is NOT the answer, so a fallback to it is
    // detectable rather than accidentally correct.
    QCOMPARE(Language(QLocale::system()).toQLocale().language(), QLocale::English);

    // Speech must follow the language actually translated into.
    QCOMPARE(window.spokenTranslationLanguage(), russian);

    // And the voice combo must have been repopulated for it. A QVoice carries
    // its own locale, so play applies the combo's voice AFTER setLanguage -
    // leaving an English voice there speaks the Russian text in English no
    // matter how correct the language is.
    QCOMPARE(window.voiceComboTranslationLanguage(), russian);
}

// The user-visible half of the same bug: after a translation the voice combo
// must list voices for the language that was translated INTO. A QVoice
// carries its own locale, so play applies the combo's voice AFTER
// setLanguage() - leaving the previous language's voices there speaks the new
// text in the old language however correct the language is.
//
// The staleness only shows on a SECOND translation that resolves elsewhere:
// the one thing refreshing these combos was
// sourceLanguagesWidget::autoLanguageChanged, and with the source unchanged
// that never fires. There is no such connection for the translation widget.
//
// Uses en->de then en->es because a machine is likelier to have those voices
// than Russian; skips when it has neither.
void MainWindowFeaturesTest::testVoiceComboOffersTheResolvedLanguagesVoices()
{
    MockHttpServer server;
    // Distinct per round: identical replies would let the second wait be
    // satisfied by the first round's text and never observe anything.
    for (int i = 0; i < 12; ++i) {
        server.queueJson(200, chatCompletionJson(QStringLiteral("translated")));
    }
    pinLocalAiSettings(server);

    const Language english(QLocale::English);
    const Language german(QLocale::German);
    const Language spanish(QLocale::Spanish);

    AppSettings settings;
    settings.setAutoTranslateEnabled(false);
    settings.setTTSProviderBackend(ATTSProvider::ProviderBackend::Qt);
    settings.setPrimaryLanguage(english);
    settings.setSecondaryLanguage(german);
    settings.setLanguages(AppSettings::Source, {english});
    settings.setLanguages(AppSettings::Translation, {english});

    MainWindow window;
    QComboBox *combo = window.translationVoiceComboBox();
    QVERIFY(combo != nullptr);

    window.sourceLanguageButtons()->checkAutoButton();
    window.translationLanguageButtons()->checkAutoButton();

    auto comboLanguages = [combo]() {
        QSet<QLocale::Language> langs;
        for (int i = 0; i < combo->count(); ++i) {
            langs.insert(combo->itemData(i).value<Voice>().language().toQLocale().language());
        }
        return langs;
    };
    // Wait on the resolved destination rather than the reply text: how many
    // requests a round makes (detection, then translation) is the provider's
    // business, so counting canned replies is not a reliable signal.
    auto translateUntil = [&](const QString &text, const Language &expected) {
        window.sourceEdit()->clear();
        typeText(window.sourceEdit(), text);
        waitForTranslateButton(window);
        QTest::mouseClick(window.translateButton(), Qt::LeftButton);
        return QTest::qWaitFor([&window, &expected]() {
            return window.spokenTranslationLanguage() == expected && !window.translationEdit()->toPlainText().isEmpty();
        },
                               8000);
    };

    QVERIFY(translateUntil(QStringLiteral("Hello"), german));
    QCOMPARE(window.spokenTranslationLanguage(), german);
    if (combo->count() == 0) {
        QSKIP("No German voices installed - cannot verify the combo's contents here");
    }
    QTRY_COMPARE(comboLanguages(), QSet<QLocale::Language>{QLocale::German});

    // Retarget: the next translation resolves to Spanish. The source stays
    // English, so nothing incidental refreshes the combos.
    settings.setSecondaryLanguage(spanish);

    QVERIFY(translateUntil(QStringLiteral("Hello again"), spanish));
    QCOMPARE(window.spokenTranslationLanguage(), spanish);
    if (combo->count() == 0) {
        QSKIP("No Spanish voices installed - cannot verify the combo's contents here");
    }
    QTRY_COMPARE(comboLanguages(), QSet<QLocale::Language>{QLocale::Spanish});
}

// Piper resolves voices by parsing model filenames, unlike Qt's
// QTextToSpeech engine - the Qt-provider version above cannot exercise that
// path. With the destination resolved to Russian (primary en == detected
// source), the translation voice combo must list only Russian voices. Skips
// when no Piper models are installed, same as the Qt variant.
void MainWindowFeaturesTest::testPiperComboOffersTheResolvedLanguagesVoices()
{
#ifdef WITH_PIPER_TTS
    MockHttpServer server;
    for (int i = 0; i < 4; ++i) {
        server.queueJson(200, chatCompletionJson(QStringLiteral("privet")));
    }
    pinLocalAiSettings(server);

    const Language english(QLocale::English);
    const Language russian(QLocale::Russian);

    AppSettings settings;
    settings.setAutoTranslateEnabled(false);
    settings.setTTSProviderBackend(ATTSProvider::ProviderBackend::Piper);
    settings.setPrimaryLanguage(english);
    settings.setSecondaryLanguage(russian);
    settings.setLanguages(AppSettings::Source, {english});
    settings.setLanguages(AppSettings::Translation, {english});

    MainWindow window;
    window.sourceLanguageButtons()->checkAutoButton();
    window.translationLanguageButtons()->checkAutoButton();

    QComboBox *combo = window.translationVoiceComboBox();
    QVERIFY(combo != nullptr);
    if (combo->count() == 0) {
        QSKIP("No Piper voices installed - cannot verify the combo's contents here");
    }

    typeText(window.sourceEdit(), QStringLiteral("Hello"));
    waitForTranslateButton(window);
    QTest::mouseClick(window.translateButton(), Qt::LeftButton);

    QVERIFY(QTest::qWaitFor([&window]() {
        return window.translationEdit()->toPlainText() == QStringLiteral("privet");
    },
                            5000));

    QCOMPARE(window.spokenTranslationLanguage(), russian);
    if (combo->count() == 0) {
        QSKIP("No Piper voices installed - cannot verify the combo's contents here");
    }
    for (int i = 0; i < combo->count(); ++i) {
        const Voice voice = combo->itemData(i).value<Voice>();
        QCOMPARE(voice.language().toQLocale().language(), QLocale::Russian);
    }
#else
    QSKIP("Piper not built");
#endif
}

// The reported regression: with the destination auto button saved as the
// checked button (CheckedTranslation=-2) and force-autodetect on, two startup
// paths used to clobber it. loadMainWindowSettings() treated QLocale::c()
// (which is what "auto" is) as "no language selected" and reset the
// destination to the system locale; validateLanguageSupport() then fell back
// to the provider's first supported locale. Both left m_destLang a real
// language, so speech followed it instead of resolving auto -> the secondary.
void MainWindowFeaturesTest::testAutoDestinationStaysAutoAtStartup()
{
    const Language english(QLocale::English);
    const Language russian(QLocale::Russian);

    AppSettings settings;
    settings.setAutoTranslateEnabled(false);
    settings.setForceSourceAutodetect(true);
    settings.setForceTranslationAutodetect(true);
    settings.setPrimaryLanguage(english);
    settings.setSecondaryLanguage(russian);
    settings.setLanguages(AppSettings::Source, {english});
    settings.setLanguages(AppSettings::Translation, {english, russian});
    settings.setCheckedButton(AppSettings::Source, LanguageButtonsWidget::autoButtonId());
    settings.setCheckedButton(AppSettings::Translation, LanguageButtonsWidget::autoButtonId());

    MainWindow window;

    // Starting with the auto destination checked must survive startup: neither
    // the settings load nor the support validation may replace it with a
    // concrete locale.
    QVERIFY(window.translationLanguageButtons()->isAutoButtonChecked());
    QCOMPARE(window.spokenTranslationLanguage(), russian);
}

// The multi-hop regression: with the destination auto, a detection-driven
// retranslate must not permanently turn m_destLang into a concrete language.
// Otherwise the NEXT auto translation resolves its destination stale and
// speaks (and populates the voice combo) for the previous translation's
// language - English voices reading Russian output.
void MainWindowFeaturesTest::testRetranslateDoesNotClobberNextAutoDestination()
{
    MockHttpServer server;
    // Attempt 1: Polish source. handleTranslationRequest resolves the
    // destination from the system locale (en -> secondary ru), then detection
    // discovers Polish and the retranslate corrects it to the primary (en).
    server.queueJson(200, chatCompletionJson(QStringLiteral("pl"))); // detection -> Polish
    server.queueJson(200, chatCompletionJson(QStringLiteral("RU-A"))); // first translate (ru)
    server.queueJson(200, chatCompletionJson(QStringLiteral("EN-A"))); // retranslate (en)
    // Attempt 2: English source. Resolves straight to the secondary (ru).
    server.queueJson(200, chatCompletionJson(QStringLiteral("en"))); // detection -> English
    server.queueJson(200, chatCompletionJson(QStringLiteral("RU-B"))); // translate (ru)
    pinLocalAiSettings(server);

    const Language english(QLocale::English);
    const Language russian(QLocale::Russian);
    const Language polish(QLocale::Polish);

    AppSettings settings;
    settings.setAutoTranslateEnabled(false);
    settings.setPrimaryLanguage(english);
    settings.setSecondaryLanguage(russian);
    settings.setDetectProvider(QStringLiteral("openai_custom"));
    settings.setDetectModel(QStringLiteral("mock-model"));
    settings.setLanguages(AppSettings::Source, {english, polish});
    settings.setLanguages(AppSettings::Translation, {english, russian});

    MainWindow window;
    window.sourceLanguageButtons()->checkAutoButton();
    window.translationLanguageButtons()->checkAutoButton();

    // Attempt 1: Polish -> retranslates to the primary (English).
    typeText(window.sourceEdit(), QStringLiteral("Dzień dobry"));
    waitForTranslateButton(window);
    QTest::mouseClick(window.translateButton(), Qt::LeftButton);
    QVERIFY(QTest::qWaitFor([&window]() {
        return window.translationEdit()->toPlainText() == QStringLiteral("EN-A");
    },
                            10000));
    QCOMPARE(window.spokenTranslationLanguage(), english);

    // Attempt 2: English -> resolves fresh to the secondary (Russian). The
    // previous retranslate must not have left a concrete destination behind.
    window.sourceEdit()->clear();
    typeText(window.sourceEdit(), QStringLiteral("Hello"));
    waitForTranslateButton(window);
    QTest::mouseClick(window.translateButton(), Qt::LeftButton);
    QVERIFY(QTest::qWaitFor([&window]() {
        return window.translationEdit()->toPlainText() == QStringLiteral("RU-B");
    },
                            10000));
    QCOMPARE(window.spokenTranslationLanguage(), russian);
}

// The pop-up showed both voice combo boxes and no playback buttons at all -
// voices to pick and nothing to play them with. It mirrored the buttons with
// isVisible() on the main window's, and a pop-up exists precisely when that
// window is hidden, where every child answers false; the combo boxes were
// never mirrored at all, so they kept the .ui default and stayed on screen.
// What each half must show is what the active TTS provider asked for.
void MainWindowFeaturesTest::testPopupShowsTheTtsControlsTheProviderAskedFor()
{
#ifndef WITH_TTS
    QSKIP("Built without TTS support - no provider asks for these controls");
#else
    AppSettings settings;
    settings.setTranslationProviderBackend(ATranslationProvider::ProviderBackend::Copy);
    settings.setTTSProviderBackend(ATTSProvider::ProviderBackend::Qt);
    // The configuration a pop-up is for: loadAppSettings() only show()s the
    // main window when it is not starting minimised to the tray, so here it
    // never appears - and every isVisible() on its children answers false.
    settings.setShowTrayIcon(true);
    settings.setStartMinimized(true);

    MainWindow window;
    QVERIFY(!window.isVisible());

    PopupWindow popup(&window);
    auto shown = [&popup](const char *name) {
        const QWidget *widget = popup.findChild<QWidget *>(QLatin1String(name));
        return widget != nullptr && !widget->isHidden();
    };

    // Both halves get their own playback controls ...
    QVERIFY(shown("sourcePlayPauseButton"));
    QVERIFY(shown("sourceStopButton"));
    QVERIFY(shown("translationPlayPauseButton"));
    QVERIFY(shown("translationStopButton"));
    // ... and the Qt backend asks for a voice per half but no speaker, so the
    // mirroring has to be per element rather than all-or-nothing.
    QVERIFY(shown("sourceVoiceComboBox"));
    QVERIFY(shown("translationVoiceComboBox"));
    QVERIFY(!shown("sourceSpeakerComboBox"));
    QVERIFY(!shown("translationSpeakerComboBox"));
    // The engine combo was never mirrored either, and Copy has no engine to
    // choose - so the pop-up showed a control the main window hides.
    QVERIFY(!shown("engineComboBox"));
#endif
}

// With no TTS provider there is nothing to play and no voice to pick.
void MainWindowFeaturesTest::testPopupHidesTtsControlsWhenThereIsNoTtsProvider()
{
    AppSettings settings;
    settings.setTTSProviderBackend(ATTSProvider::ProviderBackend::None);
    settings.setShowTrayIcon(true);
    settings.setStartMinimized(true);

    MainWindow window;
    PopupWindow popup(&window);

    for (const char *name : {"sourcePlayPauseButton",
                             "sourceStopButton",
                             "translationPlayPauseButton",
                             "translationStopButton",
                             "sourceVoiceComboBox",
                             "translationVoiceComboBox",
                             "sourceSpeakerComboBox",
                             "translationSpeakerComboBox"}) {
        const QWidget *widget = popup.findChild<QWidget *>(QLatin1String(name));
        QVERIFY2(widget != nullptr, name);
        QVERIFY2(widget->isHidden(), name);
    }
}

// Reported while spot-testing: the pop-up offered Google, Yandex and DeepL
// while the translation was going to Ollama. Its engine combo is not a fixed
// list - updateProviderUI() rebuilds it per backend, and LocalAI fills it with
// the configured local providers - but the pop-up only ever copied the
// selected INDEX, so it showed the hard-coded Mozhi list its .ui was born
// with, and choosing from it wrote that index into a combo where it meant a
// different provider.
void MainWindowFeaturesTest::testPopupEngineComboFollowsTheActiveBackend()
{
    MockHttpServer server;
    pinLocalAiSettings(server);
    AppSettings settings;
    settings.setShowTrayIcon(true);
    settings.setStartMinimized(true);

    MainWindow window;
    QComboBox *mainEngines = window.getEngineComboBox();
    QVERIFY(mainEngines != nullptr);
    QVERIFY(mainEngines->count() > 0);

    PopupWindow popup(&window);
    QComboBox *popupEngines = popup.findChild<QComboBox *>(QStringLiteral("engineComboBox"));
    QVERIFY(popupEngines != nullptr);

    // Same list, same order, same data - and the same selection.
    QCOMPARE(popupEngines->count(), mainEngines->count());
    for (int i = 0; i < mainEngines->count(); ++i) {
        QCOMPARE(popupEngines->itemText(i), mainEngines->itemText(i));
        QCOMPARE(popupEngines->itemData(i), mainEngines->itemData(i));
    }
    QCOMPARE(popupEngines->currentIndex(), mainEngines->currentIndex());

    // Explicitly: none of Mozhi's engines, which is what it used to show.
    for (int i = 0; i < popupEngines->count(); ++i) {
        QVERIFY2(popupEngines->itemText(i) != QStringLiteral("Google"), "stale Mozhi engine list in the pop-up");
        QVERIFY2(popupEngines->itemText(i) != QStringLiteral("DeepL"), "stale Mozhi engine list in the pop-up");
    }

    // And picking one in the pop-up selects that same provider in the main
    // window, rather than whatever sat at that index.
    if (popupEngines->count() > 1) {
        popupEngines->setCurrentIndex(1);
        QCOMPARE(mainEngines->currentData(), popupEngines->currentData());
    }
}

// Reported while spot-testing: the pop-up's translation voice combo offered
// Russian voices while pressing play correctly spoke English.
//
// translatorStateChanged() shows the pop-up and only THEN calls
// refreshVoicesForSpokenLanguages() - it has to, because the destination
// "auto" resolves to is not known until the translation comes back, and
// rebuilding the combos first would hold the window back behind it. So a
// pop-up built by a one-time copy is born showing the voices of the language
// translated into LAST time, while playback, which goes through the main
// window, uses the right one. The pop-up has to keep mirroring, not snapshot.
void MainWindowFeaturesTest::testPopupCombosKeepTrackingTheMainWindowAfterItIsOpen()
{
    MockHttpServer server;
    pinLocalAiSettings(server);
    AppSettings settings;
    settings.setShowTrayIcon(true);
    settings.setStartMinimized(true);

    MainWindow window;
    PopupWindow popup(&window);

    // The voice combo the report is about: refilled after the pop-up exists.
    QComboBox *mainVoices = window.translationVoiceComboBox();
    QComboBox *popupVoices = popup.findChild<QComboBox *>(QStringLiteral("translationVoiceComboBox"));
    QVERIFY(mainVoices != nullptr);
    QVERIFY(popupVoices != nullptr);

    mainVoices->clear();
    mainVoices->addItem(QStringLiteral("English voice"), QStringLiteral("en"));
    mainVoices->addItem(QStringLiteral("Another English voice"), QStringLiteral("en-2"));
    mainVoices->setCurrentIndex(1);

    QTRY_COMPARE(popupVoices->count(), 2);
    QCOMPARE(popupVoices->itemText(0), QStringLiteral("English voice"));
    QCOMPARE(popupVoices->itemData(1), mainVoices->itemData(1));
    QCOMPARE(popupVoices->currentIndex(), 1);

    // Still forwarding the other way: choosing in the pop-up selects in the
    // main window, and re-copying must not have broken that.
    popupVoices->setCurrentIndex(0);
    QCOMPARE(mainVoices->currentIndex(), 0);

    // The same has to hold for the engine list, which a provider switch
    // rebuilds underneath an open pop-up.
    QComboBox *mainEngines = window.getEngineComboBox();
    QComboBox *popupEngines = popup.findChild<QComboBox *>(QStringLiteral("engineComboBox"));
    QVERIFY(mainEngines != nullptr);
    QVERIFY(popupEngines != nullptr);

    mainEngines->clear();
    mainEngines->addItem(QStringLiteral("some-other-provider"), QStringLiteral("id"));
    QTRY_COMPARE(popupEngines->count(), 1);
    QCOMPARE(popupEngines->itemText(0), QStringLiteral("some-other-provider"));
}

// The reported sequence end to end, through the pop-up the application makes
// for itself. Same shape as testForcedAutoSpeaksTheResolvedDestinationLanguage:
// with auto on both sides the destination is not known until the translation
// comes back, so the voice combos are repopulated AFTER
// showTranslationWindow() has already built the pop-up. A pop-up that copied
// them once therefore shows the language of the previous translation - the
// user saw Russian voices offered for an English translation - while playback,
// which runs through the main window, uses the right one.
void MainWindowFeaturesTest::testPopupVoicesMatchTheMainWindowAfterARealTranslation()
{
    MockHttpServer server;
    // Detection answers first ("pl"), then the translation itself.
    for (int i = 0; i < 4; ++i) {
        server.queueJson(200, chatCompletionJson(QStringLiteral("pl")));
        server.queueJson(200, chatCompletionJson(QStringLiteral("Good morning")));
    }
    pinLocalAiSettings(server);

    const Language english(QLocale::English);
    const Language russian(QLocale::Russian);
    const Language polish(QLocale::Polish);

    AppSettings settings;
    settings.setAutoTranslateEnabled(false);
    settings.setPrimaryLanguage(english);
    settings.setSecondaryLanguage(russian);
    settings.setLanguages(AppSettings::Source, {english});
    settings.setLanguages(AppSettings::Translation, {english});
    // Detection is what makes the answer MOVE. Before the translation the
    // source can only be guessed as the system locale (English), so the
    // destination guess is the secondary, Russian. Detecting Polish makes the
    // real destination the primary, English - so the combo the pop-up copied
    // on the way up is for the wrong language, which is exactly what was
    // reported: Russian voices offered for an English translation.
    settings.setDetectProvider(QStringLiteral("openai_custom"));
    settings.setDetectModel(QStringLiteral("mock-detect"));
    settings.setTTSProviderBackend(ATTSProvider::ProviderBackend::Qt);
    settings.setWindowMode(AppSettings::PopupWindow);
    settings.setShowTrayIcon(true);
    settings.setStartMinimized(true);

    MainWindow window;
    window.hide();
    QTest::qWait(100);
    window.sourceLanguageButtons()->checkAutoButton();
    window.translationLanguageButtons()->checkAutoButton();

    QComboBox *mainVoices = window.translationVoiceComboBox();
    QVERIFY(mainVoices != nullptr);
    if (mainVoices->count() == 0) {
        QSKIP("No Qt TTS voices on this machine - an empty combo would match for the wrong reason");
    }

    // The precondition this test needs, asserted rather than assumed: with
    // nothing detected yet the destination can only be guessed, and the guess
    // is the SECONDARY. If this were already English the translation would not
    // move the combos and the test below would pass without proving anything.
    QCOMPARE(window.voiceComboTranslationLanguage(), russian);
    const QStringList voicesBeforeTranslating = [mainVoices]() {
        QStringList items;
        for (int i = 0; i < mainVoices->count(); ++i) {
            items.append(mainVoices->itemText(i));
        }
        return items;
    }();

    typeText(window.sourceEdit(), QStringLiteral("Hello"));
    waitForTranslateButton(window);
    QTest::mouseClick(window.translateButton(), Qt::LeftButton);

    PopupWindow *popup = nullptr;
    QVERIFY(QTest::qWaitFor([&window, &popup]() {
        popup = window.findChild<PopupWindow *>();
        return popup != nullptr;
    },
                            5000));
    // Wait on the resolution rather than the reply text: how many requests a
    // detect-then-translate round makes is the provider's business.
    QVERIFY2(QTest::qWaitFor([&window, english]() {
                 return window.voiceComboTranslationLanguage() == english;
             },
                             10000),
             qPrintable(QStringLiteral("destination never resolved to English (combo is %1, spoken is %2, text %3)").arg(window.voiceComboTranslationLanguage().toCode()).arg(window.spokenTranslationLanguage().toCode()).arg(window.translationEdit()->toPlainText())));

    // The main window resolved it correctly - that half already worked.
    QCOMPARE(window.spokenSourceLanguage(), polish);
    QCOMPARE(window.spokenTranslationLanguage(), english);

    // The pop-up has to be showing those same voices, not the ones the combo
    // held when it was built.
    auto *popupVoices = popup->findChild<QComboBox *>(QStringLiteral("translationVoiceComboBox"));
    QVERIFY(popupVoices != nullptr);
    auto contents = [](const QComboBox *combo) {
        QStringList items;
        for (int i = 0; i < combo->count(); ++i) {
            items.append(combo->itemText(i));
        }
        return items;
    };
    // And the list really did move, so matching means something.
    QVERIFY(contents(mainVoices) != voicesBeforeTranslating);
    QTRY_COMPARE(contents(popupVoices), contents(mainVoices));
    QCOMPARE(popupVoices->currentIndex(), mainVoices->currentIndex());
}

// The auto button read "Auto (en)" for every translation, however the
// destination actually resolved. Its label was only ever updated on the
// retranslation path - the branch taken when the first translation had gone
// to the wrong place and had to be redone - so on the common path, where the
// destination was right first time, the button kept the language it started
// with. This is the setup where no retranslation happens: source is the
// primary, so the destination falls through to the secondary immediately and
// the first translation already goes there.
void MainWindowFeaturesTest::testAutoButtonNamesTheDestinationItResolvedTo()
{
    MockHttpServer server;
    for (int i = 0; i < 4; ++i) {
        server.queueJson(200, chatCompletionJson(QStringLiteral("privet")));
    }
    pinLocalAiSettings(server);

    const Language english(QLocale::English);
    const Language russian(QLocale::Russian);

    AppSettings settings;
    settings.setAutoTranslateEnabled(false);
    settings.setPrimaryLanguage(english);
    settings.setSecondaryLanguage(russian);
    settings.setLanguages(AppSettings::Source, {english});
    settings.setLanguages(AppSettings::Translation, {english});

    MainWindow window;
    window.sourceLanguageButtons()->checkAutoButton();
    window.translationLanguageButtons()->checkAutoButton();

    typeText(window.sourceEdit(), QStringLiteral("Hello"));
    waitForTranslateButton(window);
    QTest::mouseClick(window.translateButton(), Qt::LeftButton);

    QVERIFY(QTest::qWaitFor([&window]() {
        return window.translationEdit()->toPlainText() == QStringLiteral("privet");
    },
                            5000));

    // The destination resolved to the secondary, and speech already followed
    // it. The button has to say so too.
    QCOMPARE(window.spokenTranslationLanguage(), russian);
    QTRY_COMPARE(window.translationLanguageButtons()->language(LanguageButtonsWidget::autoButtonId()), russian);
    // Still auto - naming the resolved destination must not pin it.
    QVERIFY(window.translationLanguageButtons()->isAutoButtonChecked());
}

#include "test_mainwindow_features.moc"
