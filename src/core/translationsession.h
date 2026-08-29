/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TRANSLATIONSESSION_H
#define TRANSLATIONSESSION_H

#include "language.h"
#include "translator/atranslationprovider.h"
#include "tts/attsprovider.h"

#include <QImage>
#include <QObject>
#include <QTextToSpeech>

#include <optional>

class AOcrProvider;
class LanguageResolution;
class LlmOcr;
class ModuleStatus;
class ProviderOptionsManager;
class TesseractOcr;

// Everything a frontend needs in order to translate, minus the frontend.
//
// The window used to own all of this: both providers and their backend
// choice, the options manager that fills them from settings, the language
// model, the status model, and the connection that chains a translation onto
// the next recognition. A second frontend could therefore only have those
// things by growing a window, which is why the command line has neither OCR
// nor the shared auto-language rule.
//
// Nothing here touches a widget. What comes out are signals; who draws them
// is not this class's business.
class TranslationSession : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(TranslationSession)

public:
    explicit TranslationSession(QObject *parent = nullptr);

    // --- the pieces ---------------------------------------------------------
    //
    // Both providers are replaced when the backend changes, so these are
    // borrowed for the length of one call and must not be stored. Subscribe
    // to translatorChanged()/ttsProviderChanged() instead of caching them -
    // caching is what made every swap re-do the same connection dance.
    ATranslationProvider *translator() const;
    ATTSProvider *tts() const;
    // These three outlive every swap.
    ProviderOptionsManager *options() const;
    LanguageResolution *languages() const;
    ModuleStatus *moduleStatus() const;

    // Both recognition engines are long-lived; which one is *active* follows
    // the settings and can change between one recognition and the next, so
    // ocr() is read per use rather than kept.
    AOcrProvider *ocr() const;
    TesseractOcr *tesseractOcr() const;
    LlmOcr *llmOcr() const;

    // Only meaningful once a backend has been set; before that they answer
    // with the do-nothing backend, which is also what translator() == nullptr
    // and tts() == nullptr mean.
    ATranslationProvider::ProviderBackend translationBackend() const;
    ATTSProvider::ProviderBackend ttsBackend() const;

    // --- backends -----------------------------------------------------------

    // Build the provider, or replace it when the choice moved. A no-op when
    // the backend is already the current one, exactly as the window's
    // swapTranslator()/swapTTSProvider() were.
    void setTranslationBackend(ATranslationProvider::ProviderBackend backend);
    void setTtsBackend(ATTSProvider::ProviderBackend backend);

    // Reads the backend choice out of AppSettings and applies both of the
    // above, including the TTS availability validation that can move the
    // stored choice. Kept here rather than in each frontend so a second one
    // cannot read the settings differently.
    void loadBackendsFromSettings();

    // Push the current settings into the live providers.
    void applyTranslationOptions();
    void applyTtsOptions();

    // --- languages ----------------------------------------------------------

    // The selection, plus the preference rule read from settings. The one
    // door through which a frontend's language choice enters the model.
    // "Auto" must be passed AS Language::autoLanguage(): resolving it is the
    // model's job, and a resolved value read back in pins the destination.
    void setSelectedLanguages(const Language &source, const Language &destination);

    // The destination to use when the user asked for auto.
    Language preferredDestination(const Language &source) const;

    // --- translating --------------------------------------------------------

    // destination may be Language::autoLanguage(); resolving it is this
    // class's job, not the caller's.
    void requestTranslation(const QString &text, const Language &destination, const Language &source);
    // The same, for the languages currently selected. Auto stays auto.
    void requestTranslationOfSelection(const QString &text);

    void abortTranslation();
    void acceptTranslation();
    void resetTranslator();

    // --- recognition --------------------------------------------------------

    // Fills Tesseract in from settings. Failure is reported through
    // ocrLanguagesUnavailable() rather than returned, because whether it is
    // worth mentioning is a question about the settings - a default
    // configuration that does not work is the normal state of a machine with
    // no language data installed, and saying so on every start is noise.
    void initTesseractFromSettings();

    // Reads an image file, working around PNGs Qt refuses over a private
    // chunk. Here rather than in a frontend because a screenshot tool's
    // output is no more the window's problem than the terminal's.
    static bool loadImage(const QString &path, QImage &out);

    // The common preamble for every OCR entry point: fills the LLM engine in
    // from settings and answers whether the active engine can be used at all.
    // Configuring before asking is not optional - LlmOcr::isConfigured()
    // reports the state of the last configuration, not the state of the
    // settings, so checking first rejects an engine the user has just set up.
    bool prepareOcr();

    // Chains one translation onto the next successful recognition by the
    // given engine. The recognized text reaches a frontend through the
    // engine's own recognized() signal; this only adds the translation, so
    // the text is never handled twice.
    //
    // The connection is held here rather than in a local because it has to
    // survive until it fires *or* until something cancels the capture - a
    // snip abandoned with Escape used to leave it armed, so the next
    // recognition of any kind (including a plain "recognize screen area")
    // translated unexpectedly.
    void armOcrTranslation(AOcrProvider *engine);
    void disarmOcrTranslation();
    // Whether a recognition is going to chain a translation. Callers that run
    // their own follow-up work on recognized text use this to stay out of the
    // armed path's way.
    bool isOcrTranslationArmed() const;

signals:
    // The provider object was replaced. Anything connected to the old one is
    // already gone; re-read translator()/tts() and wire up again.
    void translatorChanged();
    void ttsProviderChanged();

    // Forwarded from whichever provider is current, so a subscriber survives
    // a backend swap without reconnecting.
    void translationStateChanged(ATranslationProvider::State newState);
    void ttsStateChanged(QTextToSpeech::State newState);
    void ttsErrorOccurred(QTextToSpeech::ErrorReason reason, const QString &errorString);
    void languageDetected(const Language &detectedLanguage, bool isTranslationContext);
    void engineChanged(int engineIndex);

    // A request has gone to the provider, with the destination already
    // resolved - which is what makes it different from a frontend's "the
    // user asked for a translation", where the destination may still be auto.
    void translationStarted(const QString &text, const Language &destination, const Language &source);
    // An armed recognition is about to chain its translation. A frontend that
    // reacts to its own text-changed signals uses this to suppress the
    // duplicate it would otherwise generate.
    void ocrTranslationChaining();

    // Tesseract could not be set up with the configured languages, and the
    // configuration is not the default one - so the user chose it and wants
    // to know. How to say so is the frontend's business: the window puts it
    // in the tray, a terminal writes a line.
    void ocrLanguagesUnavailable(const QString &languages);

private:
    void connectTranslator();
    void connectTts();
    void configureLlmOcr();

    ProviderOptionsManager *m_options = nullptr;
    LanguageResolution *m_languages = nullptr;
    ModuleStatus *m_status = nullptr;
    ATranslationProvider *m_translator = nullptr;
    ATTSProvider *m_tts = nullptr;
    TesseractOcr *m_tesseractOcr = nullptr;
    LlmOcr *m_llmOcr = nullptr;
    // Deliberately not defaulted to a real backend: the setters are no-ops
    // when the backend already matches, and a default that happened to equal
    // the stored choice would skip building the provider entirely.
    std::optional<ATranslationProvider::ProviderBackend> m_translationBackend;
    std::optional<ATTSProvider::ProviderBackend> m_ttsBackend;
    QMetaObject::Connection m_ocrRecognizedConnection;
    QMetaObject::Connection m_translatorReadyConnection;
};

#endif // TRANSLATIONSESSION_H
