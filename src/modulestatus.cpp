/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "modulestatus.h"

#include "ocr/aocrprovider.h"
#include "ocr/screengrabbers/abstractscreengrabber.h"
#include "ocr/snippingarea.h"
#include "tts/attsprovider.h"
#include "tts/noopttsprovider.h"

ModuleStatus::ModuleStatus(QObject *parent)
    : QObject(parent)
{
}

ModuleStatus::Activity ModuleStatus::activity(Module module) const
{
    return m_entries[static_cast<int>(module)].activity;
}

QString ModuleStatus::message(Module module) const
{
    return messageText(m_entries[static_cast<int>(module)].message);
}

QString ModuleStatus::detail(Module module) const
{
    return m_entries[static_cast<int>(module)].detail;
}

bool ModuleStatus::isBusy() const
{
    for (const ModuleEntry &entry : m_entries) {
        if (entry.activity == Activity::Busy)
            return true;
    }
    return false;
}

bool ModuleStatus::isAvailable(Module module) const
{
    if (module != Module::Tts)
        return true;

    // NoopTTSProvider is what backend "None" creates, and it never emits
    // anything at all - the segment would sit permanently idle.
    return m_tts != nullptr && qobject_cast<NoopTTSProvider *>(m_tts) == nullptr;
}

void ModuleStatus::bindTranslator(ATranslationProvider *translator)
{
    if (m_translator != nullptr)
        disconnect(m_translator, nullptr, this, nullptr);
    m_translator = translator;
    if (translator == nullptr)
        return;

    connect(translator, &ATranslationProvider::stateChanged, this, [this](ATranslationProvider::State newState) {
        handleTranslationState(newState);
    });
    connect(translator, &ATranslationProvider::detectionStarted, this, [this]() {
        // A detection chained inside a translation must not clobber
        // "Translating" - it is part of that work, not new work.
        if (m_entries[static_cast<int>(Module::Translation)].activity != Activity::Busy) {
            m_detectionInFlight = true;
            setEntry(Module::Translation, Activity::Busy, Message::DetectingLanguage);
        }
    });
    connect(translator, &ATranslationProvider::languageDetected, this, [this]() {
        if (!m_detectionInFlight)
            return; // detection chained into a translation: "Translating" stays
        m_detectionInFlight = false;
        markIdle(Module::Translation);
    });

    // Seed by pulling: a provider swapped in mid-flight would otherwise stay
    // unreported until its next state change.
    m_detectionInFlight = false;
    setEntry(Module::Translation, Activity::Idle, Message::None);
    if (translator->getState() == ATranslationProvider::State::Processing)
        setEntry(Module::Translation, Activity::Busy, Message::Translating);
}

void ModuleStatus::bindTtsProvider(ATTSProvider *tts)
{
    if (m_tts != nullptr)
        disconnect(m_tts, nullptr, this, nullptr);
    m_tts = tts;
    if (tts == nullptr)
        return;

    connect(tts, &ATTSProvider::stateChanged, this, [this](QTextToSpeech::State newState) {
        handleTtsState(newState);
    });
    connect(tts, &ATTSProvider::errorOccurred, this, [this](QTextToSpeech::ErrorReason reason, const QString &errorString) {
        Q_UNUSED(reason)
        setEntry(Module::Tts, Activity::Error, Message::SpeechError, errorString);
    });

    seedTtsState();
}

void ModuleStatus::bindOcr(AOcrProvider *tesseract, AOcrProvider *llm)
{
    if (m_tesseractOcr != nullptr)
        disconnect(m_tesseractOcr, nullptr, this, nullptr);
    if (m_llmOcr != nullptr)
        disconnect(m_llmOcr, nullptr, this, nullptr);
    m_tesseractOcr = tesseract;
    m_llmOcr = llm;

    bindOneOcr(tesseract);
    bindOneOcr(llm);
}

void ModuleStatus::bindCapture(AbstractScreenGrabber *grabber, SnippingArea *snippingArea)
{
    if (m_grabber != nullptr)
        disconnect(m_grabber, nullptr, this, nullptr);
    if (m_snippingArea != nullptr)
        disconnect(m_snippingArea, nullptr, this, nullptr);
    m_grabber = grabber;
    m_snippingArea = snippingArea;

    if (grabber != nullptr) {
        connect(grabber, &AbstractScreenGrabber::grabbed, this, [this]() {
            setEntry(Module::Snipping, Activity::Busy, Message::SelectRegion);
        });
        connect(grabber, &AbstractScreenGrabber::grabbingFailed, this, [this]() {
            setEntry(Module::Snipping, Activity::Error, Message::CaptureFailed);
        });
    }

    if (snippingArea != nullptr) {
        connect(snippingArea, &SnippingArea::snipped, this, [this]() {
            markIdle(Module::Snipping);
        });
        connect(snippingArea, &SnippingArea::cancelled, this, [this]() {
            markIdle(Module::Snipping);
        });
    }
}

void ModuleStatus::beginScreenCapture()
{
    setEntry(Module::Snipping, Activity::Busy, Message::WaitingForCapture);
}

void ModuleStatus::setEntry(Module module, Activity activity, Message message, const QString &detail)
{
    ModuleEntry &entry = m_entries[static_cast<int>(module)];
    if (entry.activity == activity && entry.message == message && entry.detail == detail)
        return;

    entry.activity = activity;
    entry.message = message;
    entry.detail = detail;
    emit changed();
}

void ModuleStatus::markIdle(Module module)
{
    if (m_entries[static_cast<int>(module)].activity == Activity::Busy)
        setEntry(module, Activity::Idle, Message::None);
}

void ModuleStatus::bindOneOcr(AOcrProvider *engine)
{
    if (engine == nullptr)
        return;

    connect(engine, &AOcrProvider::started, this, [this]() {
        setEntry(Module::Ocr, Activity::Busy, Message::RecognizingText);
    });
    connect(engine, &AOcrProvider::recognized, this, [this]() {
        markIdle(Module::Ocr);
    });
    connect(engine, &AOcrProvider::failed, this, [this](const QString &error) {
        setEntry(Module::Ocr, Activity::Error, Message::OcrFailed, error);
    });
    connect(engine, &AOcrProvider::canceled, this, [this]() {
        markIdle(Module::Ocr);
    });
}

void ModuleStatus::handleTranslationState(ATranslationProvider::State newState)
{
    // Any state transition ends the standalone-detection story: the abort
    // and cancel paths never emit languageDetected, and this is the safety
    // net that keeps a "Detecting language" status from stranding.
    m_detectionInFlight = false;

    switch (newState) {
    case ATranslationProvider::State::Processing:
        // Busy clears a sticky error from a previous run.
        setEntry(Module::Translation, Activity::Busy, Message::Translating);
        break;
    case ATranslationProvider::State::Ready:
        markIdle(Module::Translation);
        break;
    case ATranslationProvider::State::Processed:
    case ATranslationProvider::State::Finished:
        if (m_translator == nullptr) {
            markIdle(Module::Translation);
        } else if (m_translator->error == ATranslationProvider::TranslationError::Aborted) {
            // A deliberate abort is the user's own action, not a failure.
            markIdle(Module::Translation);
        } else if (m_translator->error != ATranslationProvider::TranslationError::NoError) {
            // Sticky: Processed -> Finished -> Ready runs to completion in
            // one call stack, so a non-sticky error would be overwritten by
            // the trailing Ready before anything could repaint.
            setEntry(Module::Translation, Activity::Error, Message::TranslationFailed, m_translator->getErrorString());
        } else {
            markIdle(Module::Translation);
        }
        break;
    }
}

void ModuleStatus::handleTtsState(QTextToSpeech::State newState)
{
    switch (newState) {
    case QTextToSpeech::Speaking:
        setEntry(Module::Tts, Activity::Busy, Message::Speaking);
        break;
    case QTextToSpeech::Synthesizing:
        // MainWindow::ttsStateChanged() throws this away; for a slow network
        // synthesizer it is the only feedback there is.
        setEntry(Module::Tts, Activity::Busy, Message::PreparingSpeech);
        break;
    case QTextToSpeech::Paused:
    case QTextToSpeech::Ready:
        markIdle(Module::Tts);
        break;
    case QTextToSpeech::Error:
        setEntry(Module::Tts, Activity::Error, Message::SpeechError, m_tts != nullptr ? m_tts->errorString() : QString());
        break;
    }
}

void ModuleStatus::seedTtsState()
{
    // A freshly bound provider owes nothing to the previous one's sticky error.
    setEntry(Module::Tts, Activity::Idle, Message::None);
    if (m_tts == nullptr || !isAvailable(Module::Tts))
        return;

    // Seed by pulling ATTSProvider::state(): NoopTTSProvider never emits
    // anything, ever, so waiting for stateChanged would never converge.
    switch (m_tts->state()) {
    case QTextToSpeech::Speaking:
    case QTextToSpeech::Synthesizing:
    case QTextToSpeech::Error:
        handleTtsState(m_tts->state());
        break;
    case QTextToSpeech::Ready:
    case QTextToSpeech::Paused:
        break;
    }
}

QString ModuleStatus::messageText(Message message) const
{
    switch (message) {
    case Message::None:
        return {};
    case Message::WaitingForCapture:
        return tr("Waiting for capture");
    case Message::SelectRegion:
        return tr("Select a region");
    case Message::RecognizingText:
        return tr("Recognizing text");
    case Message::Translating:
        return tr("Translating");
    case Message::DetectingLanguage:
        return tr("Detecting language");
    case Message::PreparingSpeech:
        return tr("Preparing speech");
    case Message::Speaking:
        return tr("Speaking");
    case Message::CaptureFailed:
        return tr("Capture failed");
    case Message::OcrFailed:
        return tr("OCR failed");
    case Message::TranslationFailed:
        return tr("Translation failed");
    case Message::SpeechError:
        return tr("Speech error");
    }
    return {};
}
