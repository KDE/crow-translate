/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef MODULESTATUS_H
#define MODULESTATUS_H

#include "translator/atranslationprovider.h"

#include <QObject>
#include <QString>
#include <QTextToSpeech>

#include <array>

class ATTSProvider;
class AOcrProvider;
class AbstractScreenGrabber;
class SnippingArea;

// Aggregator for "what is currently running" across the four async
// subsystems (screen capture, OCR, translation, TTS). Both status strips -
// the main window's and the pop-up's - read this one model instead of each
// re-deriving state from the providers, since the pop-up has no providers
// of its own.
class ModuleStatus : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(ModuleStatus)

public:
    enum class Module : uint8_t { Snipping,
                                  Ocr,
                                  Translation,
                                  Tts }; // display order
    enum class Activity : uint8_t { Idle,
                                    Busy,
                                    Error };

    explicit ModuleStatus(QObject *parent = nullptr);

    Activity activity(Module module) const;
    // tr()'d on demand so a QEvent::LanguageChange re-render picks up the new
    // locale; never carries trailing dots - the view animates those.
    QString message(Module module) const;
    // Tooltip text: the full error message of the provider, not translated.
    QString detail(Module module) const;
    // Any module Busy; drives the view's ellipsis timer.
    bool isBusy() const;
    // False only for TTS when the backend is None - the view omits the
    // segment entirely rather than show a permanently idle module.
    bool isAvailable(Module module) const;

    // The translator and TTS objects are replaced on backend change
    // (MainWindow::swapTranslator()/swapTTSProvider()); both call the
    // matching bind again. The OCR engines and the grabber/snipping area are
    // long-lived and are bound once.
    void bindTranslator(ATranslationProvider *translator);
    void bindTtsProvider(ATTSProvider *tts);
    // Both engines, not activeOcr(): the active one switches per settings read.
    void bindOcr(AOcrProvider *tesseract, AOcrProvider *llm);
    void bindCapture(AbstractScreenGrabber *grabber, SnippingArea *snippingArea);

    static constexpr int moduleCount()
    {
        return s_moduleCount;
    }

public slots:
    // The one moment with no signal to hang off: AbstractScreenGrabber::grab()
    // has no "started" signal (adding one would mean touching every grabber
    // subclass), so MainWindow reports it here right before calling grab().
    void beginScreenCapture();

signals:
    void changed();

private:
    enum class Message : uint8_t {
        None,
        WaitingForCapture,
        SelectRegion,
        RecognizingText,
        Translating,
        DetectingLanguage,
        PreparingSpeech,
        Speaking,
        CaptureFailed,
        OcrFailed,
        TranslationFailed,
        SpeechError,
    };

    struct ModuleEntry {
        Activity activity = Activity::Idle;
        Message message = Message::None;
        QString detail;
    };

    void setEntry(Module module, Activity activity, Message message, const QString &detail = QString());
    // Demotes Busy to Idle but never clears a sticky Error: a translation
    // error would otherwise be overwritten by the Ready that follows in the
    // same call stack (Processed -> Finished -> reset() -> Ready runs to
    // completion before anything can repaint), and never render at all.
    void markIdle(Module module);
    void bindOneOcr(AOcrProvider *engine);
    void handleTranslationState(ATranslationProvider::State newState);
    void handleTtsState(QTextToSpeech::State newState);
    void seedTtsState();

    QString messageText(Message message) const;

    static constexpr int s_moduleCount = 4;
    std::array<ModuleEntry, s_moduleCount> m_entries;

    // Set when a standalone language detection is in flight (a detection
    // chained inside a translation is NOT tracked - "Translating" already
    // covers it). Cleared by languageDetected() or any stateChanged().
    bool m_detectionInFlight = false;

    ATranslationProvider *m_translator = nullptr;
    ATTSProvider *m_tts = nullptr;
    AOcrProvider *m_tesseractOcr = nullptr;
    AOcrProvider *m_llmOcr = nullptr;
    AbstractScreenGrabber *m_grabber = nullptr;
    SnippingArea *m_snippingArea = nullptr;
};

#endif // MODULESTATUS_H
