/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

// What "Force source language detection" and "Force translation language
// detection" do to the window when a global action starts, with both switched
// on - which is how the application is installed, and how none of the tests
// ran it: every fixture that came near these settings turned them off first.
//
// The rule itself is TranslationLogic::forcedAuto() and is tested exhaustively
// in test_translationlogic; which backend can detect what is the session's
// (test_translationsession). Here it is the buttons the user sees, and the
// request that leaves the machine:
//
//   - a backend that detects the language: both sides go to Auto;
//   - a backend that cannot (DeepL and Reverso through Mozhi, the Copy
//     backend): the source the user chose stays, since "Auto" would be sent
//     as a language the engine does not have. 3.1.0 had this check; it was
//     lost when the forcing was copied into each action in 4.0.0;
//   - speaking a selection has no destination, and leaves that side alone.

#include "language.h"
#include "languagebuttonswidget.h"
#include "mainwindow.h"
#include "mockhttpserver.h"
#include "onlinetranslator.h"
#include "selection.h"
#include "singleapplication.h"
#include "sourcetextedit.h"
#include "testdisplay.h"
#include "testisolation.h"
#include "settings/appsettings.h"
#include "translator/atranslationprovider.h"
#include "tts/attsprovider.h"

#include <QApplication>
#include <QClipboard>
#include <QDataStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QMessageBox>
#include <QTest>
#include <QTextEdit>
#include <QTimer>

namespace
{
const Language &english()
{
    static const Language language(QLocale::English);
    return language;
}

const Language &german()
{
    static const Language language(QLocale::German);
    return language;
}

const Language &french()
{
    static const Language language(QLocale::French);
    return language;
}

const Language &spanish()
{
    static const Language language(QLocale::Spanish);
    return language;
}

QByteArray mozhiAnswer(const QString &translation, const Language &detected)
{
    return QJsonDocument(QJsonObject{{QStringLiteral("translated-text"), translation}, {QStringLiteral("detected"), detected.toCode()}})
        .toJson(QJsonDocument::Compact);
}

// English chosen as the source and Spanish as the destination, neither of
// them Auto; German preferred, French for text that is German already. Both
// kinds of forcing on, as installed.
void pinLanguages()
{
    AppSettings settings;
    settings.setShowPrivacyPopup(false);
    settings.setTTSProviderBackend(ATTSProvider::ProviderBackend::None);
    settings.setAutoTranslateEnabled(false);
    settings.setForceSourceAutodetect(true);
    settings.setForceTranslationAutodetect(true);
    settings.setPrimaryLanguage(german());
    settings.setSecondaryLanguage(french());
    settings.setLanguages(AppSettings::Source, {english()});
    settings.setLanguages(AppSettings::Translation, {spanish()});
    settings.setCheckedButton(AppSettings::Source, 0);
    settings.setCheckedButton(AppSettings::Translation, 0);
}

void pinMozhi(const MockHttpServer &server, OnlineTranslator::Engine engine)
{
    pinLanguages();
    AppSettings settings;
    settings.setTranslationProviderBackend(ATranslationProvider::ProviderBackend::Mozhi);
    settings.setInstance(server.baseUrl());
    settings.setCurrentEngine(engine);
}

// Put the text where the app will look for a selection: a session with a
// clipboard manager answers from the selection buffer, a headless or offscreen
// one only has the clipboard. Before the action, since an application that is
// already active reads it there and then.
void offerSelection(const QString &text)
{
    QGuiApplication::clipboard()->setText(text, QClipboard::Clipboard);
    if (QGuiApplication::clipboard()->supportsSelection()) {
        QGuiApplication::clipboard()->setText(text, QClipboard::Selection);
    }
}

// ... and say that it is there, for the window that did not read it for
// itself. One that did has stopped listening, and this is nothing to it.
void announceSelection(const QString &text)
{
    emit Selection::instance().requestedSelectionAvailable(text);
}

QStringList translationRequests(const MockHttpServer &server)
{
    QStringList paths;
    for (int i = 0; i < server.requestCount(); ++i) {
        const QString path = server.requestPath(i);
        if (path.contains(QStringLiteral("/api/translate"))) {
            paths << path;
        }
    }
    return paths;
}

// A tenth of a second of silence, for the speech server to answer with: the
// player wants something it can play.
MockHttpServer::Response silence()
{
    const quint32 rate = 8000;
    const QByteArray samples(static_cast<qsizetype>(rate / 10 * 2), '\0');
    QByteArray wav;
    QDataStream out(&wav, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);
    out.writeRawData("RIFF", 4);
    out << static_cast<quint32>(36 + samples.size());
    out.writeRawData("WAVEfmt ", 8);
    out << static_cast<quint32>(16) << static_cast<quint16>(1) << static_cast<quint16>(1) << rate << rate * 2 << static_cast<quint16>(2)
        << static_cast<quint16>(16);
    out.writeRawData("data", 4);
    out << static_cast<quint32>(samples.size());
    out.writeRawData(samples.constData(), static_cast<int>(samples.size()));

    MockHttpServer::Response response;
    response.body = wav;
    response.headers.insert(QStringLiteral("Content-Type"), QStringLiteral("audio/wav"));
    return response;
}

// A machine with nothing to play sound on answers a speech request with a
// message box, which would wait for somebody to click it. What these tests
// are about has happened by then - the request went out - so it is closed.
void dismissMessageBoxes(QObject *owner)
{
    auto *timer = new QTimer(owner);
    QObject::connect(timer, &QTimer::timeout, owner, []() {
        for (QWidget *widget : QApplication::topLevelWidgets()) {
            if (auto *box = qobject_cast<QMessageBox *>(widget); box != nullptr && box->isVisible()) {
                box->close();
            }
        }
    });
    timer->start(100);
}

// What was asked of the speech server, which is where Mozhi's speech comes
// from: the player fetches /api/tts?engine=..&lang=..&text=.. and plays it.
QStringList speechRequests(const MockHttpServer &server)
{
    QStringList paths;
    for (int i = 0; i < server.requestCount(); ++i) {
        const QString path = server.requestPath(i);
        if (path.contains(QStringLiteral("/api/tts"))) {
            paths << path;
        }
    }
    return paths;
}
} // namespace

class MainWindowForcedAutoTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void cleanup();

    void testTranslatingASelectionSwitchesBothSidesToAuto();
    void testAnEngineThatCannotDetectKeepsTheChosenSource();
    void testTheCopyBackendKeepsTheChosenSource();
    void testSpeakingASelectionLeavesTheDestinationAlone();
    void testARecognitionSwitchesBothSidesToAuto();
    void testARecognitionKeepsTheChosenSourceOfAnEngineThatCannotDetect();
    void testSwitchedOffNothingIsForced();
    void testASelectionIsReadOutInTheLanguageItIsWrittenIn();
    void testASelectionIsReadOutInTheChosenSourceWithoutAsking();
};

void MainWindowForcedAutoTest::initTestCase()
{
    if (!hasDisplay()) {
        QSKIP("No display server available - skipping GUI tests");
    }

    Q_INIT_RESOURCE(engines);
    Q_INIT_RESOURCE(icon_theme);
}

void MainWindowForcedAutoTest::cleanupTestCase()
{
    // Leave the shared isolated settings in the state the rest of the suite
    // expects, whatever the last test happened to pin.
    AppSettings settings;
    settings.setTranslationProviderBackend(ATranslationProvider::ProviderBackend::Copy);
    settings.setTTSProviderBackend(ATTSProvider::ProviderBackend::None);
    settings.setCurrentEngine(OnlineTranslator::Google);
}

void MainWindowForcedAutoTest::cleanup()
{
    QApplication::clipboard()->clear();
}

// The case everybody has: an engine that detects. Both sides go to Auto, the
// text is sent as "auto", and the destination is the preferred language.
void MainWindowForcedAutoTest::testTranslatingASelectionSwitchesBothSidesToAuto()
{
    MockHttpServer server;
    for (int i = 0; i < 3; ++i) {
        server.queueJson(200, mozhiAnswer(QStringLiteral("eine Auswahl"), english()));
    }
    pinMozhi(server, OnlineTranslator::Google);

    MainWindow window;
    QVERIFY(!window.sourceLanguageButtons()->isAutoButtonChecked());
    QVERIFY(!window.translationLanguageButtons()->isAutoButtonChecked());

    offerSelection(QStringLiteral("a selection"));
    window.translateSelection();
    QVERIFY(window.sourceLanguageButtons()->isAutoButtonChecked());
    QVERIFY(window.translationLanguageButtons()->isAutoButtonChecked());

    announceSelection(QStringLiteral("a selection"));
    QTRY_COMPARE_WITH_TIMEOUT(window.translationEdit()->toPlainText(), QStringLiteral("eine Auswahl"), 5000);

    const QStringList requests = translationRequests(server);
    QVERIFY(!requests.isEmpty());
    for (const QString &request : requests) {
        QVERIFY2(request.contains(QStringLiteral("from=auto")), qPrintable(request));
    }
    QVERIFY2(requests.constLast().contains(QStringLiteral("to=de")), qPrintable(requests.constLast()));
}

// DeepL has no "auto". With the source forced to Auto the request went out as
// from=auto and came back as an error, for as long as the setting was on -
// and it is on unless the user finds it and turns it off.
void MainWindowForcedAutoTest::testAnEngineThatCannotDetectKeepsTheChosenSource()
{
    MockHttpServer server;
    for (int i = 0; i < 3; ++i) {
        server.queueJson(200, mozhiAnswer(QStringLiteral("eine Auswahl"), english()));
    }
    pinMozhi(server, OnlineTranslator::Deepl);

    MainWindow window;
    offerSelection(QStringLiteral("a selection"));
    window.translateSelection();
    QVERIFY2(!window.sourceLanguageButtons()->isAutoButtonChecked(), "the source was forced to Auto for an engine that cannot detect");
    QCOMPARE(window.sourceLanguageButtons()->checkedId(), 0);
    // The destination needs nothing of the engine: it is this application's
    // own rule, applied to the source the user chose.
    QVERIFY(window.translationLanguageButtons()->isAutoButtonChecked());

    announceSelection(QStringLiteral("a selection"));
    QTRY_COMPARE_WITH_TIMEOUT(window.translationEdit()->toPlainText(), QStringLiteral("eine Auswahl"), 5000);

    const QStringList requests = translationRequests(server);
    QVERIFY(!requests.isEmpty());
    for (const QString &request : requests) {
        QVERIFY2(!request.contains(QStringLiteral("from=auto")), qPrintable(request));
        QVERIFY2(request.contains(QStringLiteral("from=en")), qPrintable(request));
    }
    QVERIFY2(requests.constLast().contains(QStringLiteral("to=de")), qPrintable(requests.constLast()));
}

// The backend an installation starts with before one is chosen. It copies, and
// only between a language and itself - it has nothing to detect with either.
void MainWindowForcedAutoTest::testTheCopyBackendKeepsTheChosenSource()
{
    pinLanguages();
    AppSettings().setTranslationProviderBackend(ATranslationProvider::ProviderBackend::Copy);

    MainWindow window;
    offerSelection(QStringLiteral("a selection"));
    window.translateSelection();
    announceSelection(QStringLiteral("a selection"));

    QVERIFY(!window.sourceLanguageButtons()->isAutoButtonChecked());
    QCOMPARE(window.sourceLanguageButtons()->checkedId(), 0);
    QVERIFY(window.translationLanguageButtons()->isAutoButtonChecked());
    QTRY_COMPARE_WITH_TIMEOUT(window.sourceEdit()->toPlainText(), QStringLiteral("a selection"), 5000);
}

// Nothing is translated when a selection is read out, so there is no
// destination to force - and the one the user chose is still chosen when they
// come back to the window.
void MainWindowForcedAutoTest::testSpeakingASelectionLeavesTheDestinationAlone()
{
    MockHttpServer server;
    pinMozhi(server, OnlineTranslator::Google);

    MainWindow window;
    offerSelection(QStringLiteral("a selection"));
    window.speakSelection();
    QVERIFY(window.sourceLanguageButtons()->isAutoButtonChecked());
    QVERIFY(!window.translationLanguageButtons()->isAutoButtonChecked());
    QCOMPARE(window.translationLanguageButtons()->checkedId(), 0);
    announceSelection(QStringLiteral("a selection"));
}

// A recognition, started the way its shortcut starts it. The delay is there so
// that nothing is captured: the forcing happens when the action starts, and
// the window is gone again long before the timer.
void MainWindowForcedAutoTest::testARecognitionSwitchesBothSidesToAuto()
{
    MockHttpServer server;
    pinMozhi(server, OnlineTranslator::Google);
    AppSettings settings;
    settings.setOcrEngine(AppSettings::OcrEngine::Llm);
    settings.setOcrLlmProvider(QStringLiteral("openai_custom"));
    settings.setOcrLlmUrl(QStringLiteral("openai_custom"), server.baseUrl());
    settings.setOcrLlmModel(QStringLiteral("openai_custom"), QStringLiteral("mock-model"));
    settings.setCaptureDelay(600000);

    MainWindow window;
    window.delayedRecognizeScreenArea();
    QVERIFY(window.sourceLanguageButtons()->isAutoButtonChecked());
    QVERIFY(window.translationLanguageButtons()->isAutoButtonChecked());
}

void MainWindowForcedAutoTest::testARecognitionKeepsTheChosenSourceOfAnEngineThatCannotDetect()
{
    MockHttpServer server;
    pinMozhi(server, OnlineTranslator::Deepl);
    AppSettings settings;
    settings.setOcrEngine(AppSettings::OcrEngine::Llm);
    settings.setOcrLlmProvider(QStringLiteral("openai_custom"));
    settings.setOcrLlmUrl(QStringLiteral("openai_custom"), server.baseUrl());
    settings.setOcrLlmModel(QStringLiteral("openai_custom"), QStringLiteral("mock-model"));
    settings.setCaptureDelay(600000);

    MainWindow window;
    window.delayedTranslateScreenArea();
    QVERIFY(!window.sourceLanguageButtons()->isAutoButtonChecked());
    QCOMPARE(window.sourceLanguageButtons()->checkedId(), 0);
    QVERIFY(window.translationLanguageButtons()->isAutoButtonChecked());
}

// Both settings off: a global action works with the languages as they are.
void MainWindowForcedAutoTest::testSwitchedOffNothingIsForced()
{
    MockHttpServer server;
    for (int i = 0; i < 3; ++i) {
        server.queueJson(200, mozhiAnswer(QStringLiteral("una selección"), english()));
    }
    pinMozhi(server, OnlineTranslator::Google);
    {
        AppSettings settings;
        settings.setForceSourceAutodetect(false);
        settings.setForceTranslationAutodetect(false);
    }

    MainWindow window;
    offerSelection(QStringLiteral("a selection"));
    window.translateSelection();
    QVERIFY(!window.sourceLanguageButtons()->isAutoButtonChecked());
    QVERIFY(!window.translationLanguageButtons()->isAutoButtonChecked());

    announceSelection(QStringLiteral("a selection"));
    QTRY_COMPARE_WITH_TIMEOUT(window.translationEdit()->toPlainText(), QStringLiteral("una selección"), 5000);
    const QStringList requests = translationRequests(server);
    QVERIFY(!requests.isEmpty());
    QVERIFY2(requests.constLast().contains(QStringLiteral("from=en")) && requests.constLast().contains(QStringLiteral("to=es")),
             qPrintable(requests.constLast()));
}

// The speak-selection shortcut over German words, after an English text has
// been translated. 3.1.0 found out what language the selection was in and
// then read it; since 4.0.0 it was read at once, in whatever the source side
// had last come to - English here, with an English voice.
void MainWindowForcedAutoTest::testASelectionIsReadOutInTheLanguageItIsWrittenIn()
{
    MockHttpServer server;
    pinMozhi(server, OnlineTranslator::Google);
    AppSettings().setTTSProviderBackend(ATTSProvider::ProviderBackend::Mozhi);

    MainWindow window;
    server.queueJson(200, mozhiAnswer(QStringLiteral("ein Text"), english()));
    offerSelection(QStringLiteral("a text"));
    window.translateSelection();
    announceSelection(QStringLiteral("a text"));
    QTRY_COMPARE_WITH_TIMEOUT(window.translationEdit()->toPlainText(), QStringLiteral("ein Text"), 5000);
    QCOMPARE(window.spokenSourceLanguage(), english());
    QVERIFY(speechRequests(server).isEmpty());

    server.queueJson(200, mozhiAnswer(QString(), german())); // what language is this?
    server.queueResponse(silence()); // and the speech
    dismissMessageBoxes(&window);
    offerSelection(QStringLiteral("Guten Tag zusammen"));
    window.speakSelection();
    announceSelection(QStringLiteral("Guten Tag zusammen"));

    QTRY_VERIFY_WITH_TIMEOUT(!speechRequests(server).isEmpty(), 8000);
    const QString request = speechRequests(server).constFirst();
    QVERIFY2(request.contains(QStringLiteral("lang=de")), qPrintable(request));
    QVERIFY2(request.contains(QStringLiteral("Guten")), qPrintable(request));
    // The window agrees with what was said: the source is German now, and so
    // are the voices its play button would use.
    QCOMPARE(window.spokenSourceLanguage(), german());
    QCOMPARE(window.voiceComboSourceLanguage(), german());
    // The translation on screen is still the one into German.
    QCOMPARE(window.spokenTranslationLanguage(), german());
}

// With a source chosen and nothing forced there is nothing to find out, and
// the text goes nowhere but to the speech.
void MainWindowForcedAutoTest::testASelectionIsReadOutInTheChosenSourceWithoutAsking()
{
    MockHttpServer server;
    pinMozhi(server, OnlineTranslator::Google);
    {
        AppSettings settings;
        settings.setTTSProviderBackend(ATTSProvider::ProviderBackend::Mozhi);
        settings.setForceSourceAutodetect(false);
    }

    MainWindow window;
    server.queueResponse(silence());
    dismissMessageBoxes(&window);
    offerSelection(QStringLiteral("a selection"));
    window.speakSelection();
    announceSelection(QStringLiteral("a selection"));

    QTRY_VERIFY_WITH_TIMEOUT(!speechRequests(server).isEmpty(), 8000);
    const QString request = speechRequests(server).constFirst();
    QVERIFY2(request.contains(QStringLiteral("lang=en")), qPrintable(request));
    QVERIFY2(translationRequests(server).isEmpty(), qPrintable(translationRequests(server).join(QLatin1Char('\n'))));
}

int main(int argc, char *argv[])
{
    isolateTestSettings();

    SingleApplication app(argc, argv, true);
    MainWindowForcedAutoTest tc;
    QTEST_SET_MAIN_SOURCE_PATH
    return QTest::qExec(&tc, argc, argv);
}

#include "test_mainwindow_forcedauto.moc"
