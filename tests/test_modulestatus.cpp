/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

// Headless contract tests for the ModuleStatus aggregator: every provider
// signal is driven directly (they are public), TTS is exercised through the
// pull-seeded bind path because ATTSProvider::stateChanged is a private
// signal. Nothing is ever shown, so no display server is needed beyond what
// QApplication itself requires.

#include "language.h"
#include "modulestatus.h"
#include "provideroptions.h"
#include "ocr/aocrprovider.h"
#include "ocr/screengrabbers/genericscreengrabber.h"
#include "ocr/snippingarea.h"
#include "translator/atranslationprovider.h"
#include "translator/copytranslationprovider.h"
#include "tts/attsprovider.h"
#include "tts/noopttsprovider.h"
#include "tts/voice.h"

#include <QApplication>
#include <QImage>
#include <QPixmap>
#include <QScreen>
#include <QSignalSpy>
#include <QTest>
#include <QTextToSpeech>

// The smallest AOcrProvider implementation; the model only ever talks to the
// base-class signals, and driving those directly keeps this test independent
// of Tesseract/LLM setup.
class StubOcr : public AOcrProvider
{
    Q_OBJECT
    Q_DISABLE_COPY(StubOcr)

public:
    using AOcrProvider::AOcrProvider;

    QString engineName() const override
    {
        return QStringLiteral("stub");
    }
    bool isConfigured() const override
    {
        return true;
    }
    void recognize(const QImage &, int) override
    {
        emit started();
    }
    void cancel() override
    {
    }
};

// The smallest ATTSProvider implementation that can report a chosen state;
// ATTSProvider::stateChanged is private, so pull-seeding is the only
// observable path for bindTtsProvider() anyway.
class StubTtsProvider : public ATTSProvider
{
    Q_OBJECT
    Q_DISABLE_COPY(StubTtsProvider)

public:
    explicit StubTtsProvider(QTextToSpeech::State state, QObject *parent = nullptr)
        : ATTSProvider(parent)
        , m_state(state)
    {
    }

    QString getProviderType() const override
    {
        return QStringLiteral("StubTtsProvider");
    }
    void say(const QString &) override
    {
    }
    void stop() override
    {
    }
    void pause() override
    {
    }
    void resume() override
    {
    }
    QTextToSpeech::State state() const override
    {
        return m_state;
    }
    QTextToSpeech::ErrorReason errorReason() const override
    {
        return QTextToSpeech::ErrorReason::NoError;
    }
    QString errorString() const override
    {
        return QStringLiteral("stub tts failure");
    }
    Language language() const override
    {
        return Language::autoLanguage();
    }
    void setLanguage(const Language &) override
    {
    }
    Voice voice() const override
    {
        return {};
    }
    void setVoice(const Voice &) override
    {
    }
    QList<Voice> availableVoices() const override
    {
        return {};
    }
    QList<Voice> findVoices(const Language &) const override
    {
        return {};
    }
    double rate() const override
    {
        return 0.0;
    }
    void setRate(double) override
    {
    }
    double pitch() const override
    {
        return 0.0;
    }
    void setPitch(double) override
    {
    }
    double volume() const override
    {
        return 1.0;
    }
    void setVolume(double) override
    {
    }
    QList<Language> availableLanguages() const override
    {
        return {};
    }
    void applyOptions(const ProviderOptions &) override
    {
    }
    std::unique_ptr<ProviderOptions> getDefaultOptions() const override
    {
        return std::make_unique<ProviderOptions>();
    }
    QStringList getAvailableOptions() const override
    {
        return {};
    }
    ProviderUIRequirements getUIRequirements() const override
    {
        return {};
    }
    QStringList availableSpeakers() const override
    {
        return {};
    }
    QStringList availableSpeakersForVoice(const Voice &) const override
    {
        return {};
    }
    QString currentSpeaker() const override
    {
        return {};
    }
    void setSpeaker(const QString &) override
    {
    }

private:
    QTextToSpeech::State m_state;
};

class ModuleStatusTest : public QObject
{
    Q_OBJECT

private slots:
    void testTranslationStates();
    void testTranslationErrorStickiness();
    void testTranslationAbortIsNotAnError();
    void testDetectionStates();
    void testOcrStates();
    void testBothOcrEnginesShareOneSegment();
    void testCaptureStates();
    void testTtsPullSeeding();
    void testTtsNoopUnavailable();
    void testIsBusy();
    void testChangedSignal();
    void testRebindingClearsStickyState();

private:
    static constexpr auto s_translating = ModuleStatus::Module::Translation;
    static constexpr auto s_ocr = ModuleStatus::Module::Ocr;
    static constexpr auto s_snipping = ModuleStatus::Module::Snipping;
    static constexpr auto s_tts = ModuleStatus::Module::Tts;
};

// Copy's translate() drives the whole state machine synchronously: success is
// Processing -> Processed/NoError, a language mismatch is Processing ->
// Finished/UnsupportedDstLanguage. "Busy" is only observable during the
// emission itself, so record it from a stateChanged handler connected after
// the model's own (same-emitter direct connections run in creation order).
void ModuleStatusTest::testTranslationStates()
{
    ModuleStatus status;
    CopyTranslationProvider translator;

    QSignalSpy changedSpy(&status, &ModuleStatus::changed);
    status.bindTranslator(&translator);
    QVERIFY(changedSpy.isEmpty()); // Ready seeds Idle, not Busy

    ModuleStatus::Activity seenWhileProcessing = ModuleStatus::Activity::Idle;
    QString messageWhileProcessing;
    connect(&translator, &ATranslationProvider::stateChanged, &translator, [&](ATranslationProvider::State state) {
        if (state == ATranslationProvider::State::Processing) {
            seenWhileProcessing = status.activity(s_translating);
            messageWhileProcessing = status.message(s_translating);
        }
    });

    const Language lang(QLocale::system());
    translator.translate(QStringLiteral("hello"), lang, lang);
    QCOMPARE(seenWhileProcessing, ModuleStatus::Activity::Busy);
    QCOMPARE(messageWhileProcessing, QStringLiteral("Translating"));

    // The synchronous cascade to Processed/Finished/Ready leaves it Idle.
    QCOMPARE(status.activity(s_translating), ModuleStatus::Activity::Idle);
    QVERIFY(status.message(s_translating).isEmpty());

    translator.reset();
    QCOMPARE(status.activity(s_translating), ModuleStatus::Activity::Idle);
}

void ModuleStatusTest::testTranslationErrorStickiness()
{
    ModuleStatus status;
    CopyTranslationProvider translator;
    status.bindTranslator(&translator);

    // Finished-with-error: the source/destination mismatch path.
    translator.translate(QStringLiteral("hello"), Language(QLocale::English), Language(QLocale::French));
    QCOMPARE(status.activity(s_translating), ModuleStatus::Activity::Error);
    QCOMPARE(status.message(s_translating), QStringLiteral("Translation failed"));
    QVERIFY(!status.detail(s_translating).isEmpty());

    // The synchronous trailing Ready (reset()) must not overwrite the error.
    translator.reset();
    QCOMPARE(status.activity(s_translating), ModuleStatus::Activity::Error);

    // The next Busy clears it.
    const Language lang(QLocale::system());
    translator.translate(QStringLiteral("hello"), lang, lang);
    QCOMPARE(status.activity(s_translating), ModuleStatus::Activity::Idle);

    translator.reset();
    QCOMPARE(status.activity(s_translating), ModuleStatus::Activity::Idle);
}

// reset() on a Processed provider goes through abort(): Finished/Aborted ->
// Ready. A deliberate abort is not a failure and must never read as Error -
// not even transiently, during the Finished emission itself.
void ModuleStatusTest::testTranslationAbortIsNotAnError()
{
    ModuleStatus status;
    CopyTranslationProvider translator;
    status.bindTranslator(&translator);

    ModuleStatus::Activity seenAtFinished = ModuleStatus::Activity::Idle;
    connect(&translator, &ATranslationProvider::stateChanged, &translator, [&](ATranslationProvider::State state) {
        if (state == ATranslationProvider::State::Finished)
            seenAtFinished = status.activity(s_translating);
    });

    const Language lang(QLocale::system());
    translator.translate(QStringLiteral("hello"), lang, lang);
    QCOMPARE(translator.getState(), ATranslationProvider::State::Processed);

    translator.reset();
    QCOMPARE(translator.error, ATranslationProvider::TranslationError::NoError); // reset() clears it
    QCOMPARE(seenAtFinished, ModuleStatus::Activity::Idle);
    QCOMPARE(status.activity(s_translating), ModuleStatus::Activity::Idle);
}

// Language detection in the strip: a standalone detection is the only
// translation-module work in flight and shows as Busy "Detecting language";
// a detection chained inside a translation must not clobber "Translating";
// any stateChanged is the abort/cancel safety net; and like any Busy it
// clears a sticky error.
void ModuleStatusTest::testDetectionStates()
{
    ModuleStatus status;
    CopyTranslationProvider translator;
    status.bindTranslator(&translator);

    emit translator.detectionStarted();
    QCOMPARE(status.activity(s_translating), ModuleStatus::Activity::Busy);
    QCOMPARE(status.message(s_translating), QStringLiteral("Detecting language"));

    emit translator.languageDetected(Language(QLocale::English), false);
    QCOMPARE(status.activity(s_translating), ModuleStatus::Activity::Idle);

    // Chained detection (detect-then-translate): "Translating" is already
    // up, so detectionStarted is not new work, and its languageDetected
    // must not end the translation.
    emit translator.stateChanged(ATranslationProvider::State::Processing);
    QCOMPARE(status.message(s_translating), QStringLiteral("Translating"));
    emit translator.detectionStarted();
    QCOMPARE(status.message(s_translating), QStringLiteral("Translating"));
    emit translator.languageDetected(Language(QLocale::English), true);
    QCOMPARE(status.message(s_translating), QStringLiteral("Translating"));

    // Safety net: a state transition with no languageDetected at all (the
    // abort path) still demotes the detection Busy.
    emit translator.stateChanged(ATranslationProvider::State::Ready);
    QCOMPARE(status.activity(s_translating), ModuleStatus::Activity::Idle);

    // A detection Busy clears a sticky error, like any other Busy.
    translator.translate(QStringLiteral("hello"), Language(QLocale::English), Language(QLocale::French));
    QCOMPARE(status.activity(s_translating), ModuleStatus::Activity::Error);
    emit translator.detectionStarted();
    QCOMPARE(status.activity(s_translating), ModuleStatus::Activity::Busy);
    QCOMPARE(status.message(s_translating), QStringLiteral("Detecting language"));
    emit translator.languageDetected(Language(QLocale::English), false);
    QCOMPARE(status.activity(s_translating), ModuleStatus::Activity::Idle);
}

void ModuleStatusTest::testOcrStates()
{
    ModuleStatus status;
    StubOcr engine;
    status.bindOcr(&engine, nullptr);

    QCOMPARE(status.activity(s_ocr), ModuleStatus::Activity::Idle);

    engine.recognize(QImage(), 96);
    QCOMPARE(status.activity(s_ocr), ModuleStatus::Activity::Busy);
    QCOMPARE(status.message(s_ocr), QStringLiteral("Recognizing text"));

    emit engine.canceled();
    QCOMPARE(status.activity(s_ocr), ModuleStatus::Activity::Idle);

    emit engine.recognized(QStringLiteral("text"));
    QCOMPARE(status.activity(s_ocr), ModuleStatus::Activity::Idle);

    engine.recognize(QImage(), 96);
    emit engine.failed(QStringLiteral("boom"));
    QCOMPARE(status.activity(s_ocr), ModuleStatus::Activity::Error);
    QCOMPARE(status.message(s_ocr), QStringLiteral("OCR failed"));
    QCOMPARE(status.detail(s_ocr), QStringLiteral("boom"));

    // Sticky until the next Busy.
    emit engine.recognized(QStringLiteral("text"));
    QCOMPARE(status.activity(s_ocr), ModuleStatus::Activity::Error);

    engine.recognize(QImage(), 96);
    QCOMPARE(status.activity(s_ocr), ModuleStatus::Activity::Busy);

    emit engine.recognized(QStringLiteral("text"));
    QCOMPARE(status.activity(s_ocr), ModuleStatus::Activity::Idle);
}

void ModuleStatusTest::testCaptureStates()
{
    ModuleStatus status;
    GenericScreenGrabber grabber;
    SnippingArea snippingArea;
    status.bindCapture(&grabber, &snippingArea);

    QCOMPARE(status.activity(s_snipping), ModuleStatus::Activity::Idle);

    status.beginScreenCapture();
    QCOMPARE(status.activity(s_snipping), ModuleStatus::Activity::Busy);
    QCOMPARE(status.message(s_snipping), QStringLiteral("Waiting for capture"));

    emit grabber.grabbed({});
    QCOMPARE(status.activity(s_snipping), ModuleStatus::Activity::Busy);
    QCOMPARE(status.message(s_snipping), QStringLiteral("Select a region"));

    emit snippingArea.snipped(QPixmap(), 96);
    QCOMPARE(status.activity(s_snipping), ModuleStatus::Activity::Idle);

    status.beginScreenCapture();
    emit snippingArea.cancelled();
    QCOMPARE(status.activity(s_snipping), ModuleStatus::Activity::Idle);

    status.beginScreenCapture();
    emit grabber.grabbingFailed();
    QCOMPARE(status.activity(s_snipping), ModuleStatus::Activity::Error);
    QCOMPARE(status.message(s_snipping), QStringLiteral("Capture failed"));

    // The error survives the next idle transition and clears on the next Busy.
    emit snippingArea.cancelled();
    QCOMPARE(status.activity(s_snipping), ModuleStatus::Activity::Error);
    status.beginScreenCapture();
    QCOMPARE(status.activity(s_snipping), ModuleStatus::Activity::Busy);
}

// ATTSProvider::stateChanged is private, so bindTtsProvider()'s pull-seeding
// is the only path a test can observe.
void ModuleStatusTest::testTtsPullSeeding()
{
    ModuleStatus status;

    StubTtsProvider speaking(QTextToSpeech::Speaking);
    status.bindTtsProvider(&speaking);
    QVERIFY(status.isAvailable(s_tts));
    QCOMPARE(status.activity(s_tts), ModuleStatus::Activity::Busy);
    QCOMPARE(status.message(s_tts), QStringLiteral("Speaking"));

    StubTtsProvider synthesizing(QTextToSpeech::Synthesizing);
    status.bindTtsProvider(&synthesizing);
    QCOMPARE(status.activity(s_tts), ModuleStatus::Activity::Busy);
    QCOMPARE(status.message(s_tts), QStringLiteral("Preparing speech"));

    StubTtsProvider ready(QTextToSpeech::Ready);
    status.bindTtsProvider(&ready);
    QCOMPARE(status.activity(s_tts), ModuleStatus::Activity::Idle);

    StubTtsProvider failed(QTextToSpeech::Error);
    status.bindTtsProvider(&failed);
    QCOMPARE(status.activity(s_tts), ModuleStatus::Activity::Error);
    QCOMPARE(status.detail(s_tts), QStringLiteral("stub tts failure"));
}

void ModuleStatusTest::testTtsNoopUnavailable()
{
    ModuleStatus status;
    NoopTTSProvider noop;

    QVERIFY(!status.isAvailable(s_tts)); // not bound at all
    status.bindTtsProvider(&noop);
    QVERIFY(!status.isAvailable(s_tts));
    QCOMPARE(status.activity(s_tts), ModuleStatus::Activity::Idle);
}

void ModuleStatusTest::testIsBusy()
{
    ModuleStatus status;
    QVERIFY(!status.isBusy());

    StubTtsProvider speaking(QTextToSpeech::Speaking);
    status.bindTtsProvider(&speaking);
    QVERIFY(status.isBusy());

    StubTtsProvider ready(QTextToSpeech::Ready);
    status.bindTtsProvider(&ready);
    QVERIFY(!status.isBusy());

    // An Error is not Busy - the ellipsis timer must not run for it.
    CopyTranslationProvider translator;
    status.bindTranslator(&translator);
    translator.translate(QStringLiteral("hello"), Language(QLocale::English), Language(QLocale::French));
    QCOMPARE(status.activity(s_translating), ModuleStatus::Activity::Error);
    QVERIFY(!status.isBusy());
}

void ModuleStatusTest::testChangedSignal()
{
    ModuleStatus status;
    QSignalSpy changedSpy(&status, &ModuleStatus::changed);

    status.beginScreenCapture();
    QCOMPARE(changedSpy.count(), 1);

    // Redundant transitions must not spam the view into repainting.
    status.beginScreenCapture();
    QCOMPARE(changedSpy.count(), 1);
}

// The strip has a single OCR segment, but both engines are bound (the active
// one switches per settings read). A terminal from the engine that didn't
// start the run still clears the segment - the engines never run
// concurrently, so any terminal ends "recognizing".
void ModuleStatusTest::testBothOcrEnginesShareOneSegment()
{
    ModuleStatus status;
    StubOcr tesseract;
    StubOcr llm;
    status.bindOcr(&tesseract, &llm);

    tesseract.recognize(QImage(), 96);
    QCOMPARE(status.activity(s_ocr), ModuleStatus::Activity::Busy);

    emit llm.recognized(QStringLiteral("text"));
    QCOMPARE(status.activity(s_ocr), ModuleStatus::Activity::Idle);

    llm.recognize(QImage(), 96);
    QCOMPARE(status.activity(s_ocr), ModuleStatus::Activity::Busy);

    emit tesseract.canceled();
    QCOMPARE(status.activity(s_ocr), ModuleStatus::Activity::Idle);
}

// swapTranslator()/swapTTSProvider() rebind mid-flight: a freshly bound
// provider owes nothing to its predecessor's sticky error or busy state.
void ModuleStatusTest::testRebindingClearsStickyState()
{
    ModuleStatus status;

    CopyTranslationProvider failing;
    status.bindTranslator(&failing);
    failing.translate(QStringLiteral("hello"), Language(QLocale::English), Language(QLocale::French));
    QCOMPARE(status.activity(s_translating), ModuleStatus::Activity::Error);

    CopyTranslationProvider fresh;
    status.bindTranslator(&fresh);
    QCOMPARE(status.activity(s_translating), ModuleStatus::Activity::Idle);
    QVERIFY(status.message(s_translating).isEmpty());

    StubTtsProvider speaking(QTextToSpeech::Speaking);
    status.bindTtsProvider(&speaking);
    QCOMPARE(status.activity(s_tts), ModuleStatus::Activity::Busy);

    StubTtsProvider ready(QTextToSpeech::Ready);
    status.bindTtsProvider(&ready);
    QCOMPARE(status.activity(s_tts), ModuleStatus::Activity::Idle);
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    ModuleStatusTest tc;
    QTEST_SET_MAIN_SOURCE_PATH
    return QTest::qExec(&tc, argc, argv);
}

#include "test_modulestatus.moc"
