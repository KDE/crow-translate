/*
 * SPDX-FileCopyrightText: 2025 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef ATRANSLATIONPROVIDER_H
#define ATRANSLATIONPROVIDER_H

#include "language.h"
#include "provideroptions.h"
#include "core/translationresult.h"

#include <QList>
#include <QLocale>
#include <QObject>

#include <memory>

class ProviderOptions;

class ATranslationProvider : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(ATranslationProvider)
public:
    enum class TranslationError : uint8_t {
        NoError,
        UnsupportedSrcLanguage,
        UnsupportedDstLanguage,
        Aborted,
        Custom
    };
    enum class ProviderBackend : uint8_t {
        Copy,
        Mozhi,
        LocalAI
    };
    Q_ENUM(ProviderBackend)

    enum class State : uint8_t {
        Ready,
        Processing,
        Processed,
        Finished
    };

    static ATranslationProvider *createTranslationProvider(QObject *parent = nullptr, ProviderBackend chosenBackend = ProviderBackend::Copy);
    static void resetProblematicProvider(ProviderBackend backend);
    virtual QString getProviderType() const = 0;
    virtual State getState();
    virtual void abort();
    virtual void reset();
    virtual void finish();
    virtual QVector<Language> supportedSourceLanguages() = 0;
    virtual QVector<Language> supportedDestinationLanguages() = 0;
    virtual bool supportsAutodetection() const = 0;
    virtual Language detectLanguage(const QString &text) = 0;
    virtual QString getErrorString();

    // For whoever asked for the translation, from its languageDetected()
    // handler: the destination it asked for was chosen before anyone knew what
    // language the text is in, and now that the source is known it should
    // have been `destination`.
    //
    // A backend that finds the language out first and translates afterwards
    // (LocalAI with a detection model) has not asked for the translation yet
    // when it reports the detection: it takes the new destination and answers
    // true. One whose detection arrives with the translation (Mozhi) has
    // nothing left to redirect and answers false; the caller asks again.
    virtual bool redirect(const Language &destination);

    // Options system
    virtual void applyOptions(const ProviderOptions &options) = 0;
    virtual std::unique_ptr<ProviderOptions> getDefaultOptions() const = 0;
    virtual QStringList getAvailableOptions() const = 0;

    // What this backend can do; see ProviderCapability.
    virtual ProviderCapabilities capabilities() const = 0;

    // Settings integration (type-safe)
    virtual void saveOptionToSettings(const QString &optionKey, const QVariant &value) = 0;

    TranslationError error = TranslationError::NoError;
    Language sourceLanguage = Language::autoLanguage();
    Language translationLanguage = Language::autoLanguage();
    TranslationResult result;

public slots:
    virtual void translate(const QString &inputText, const Language &translationLang, const Language &sourceLang) = 0;

private:
protected:
    State state = State::Ready;
    QString errorString;
    explicit ATranslationProvider(QObject *parent = nullptr);

signals:
    void stateChanged(State newState);
    // The translation so far, as text (rendering is the frontend's job, exactly
    // as it is for `result`). Emitted between the request and the final
    // stateChanged(Processed), which carries the complete text.
    void partialTranslation(const QString &text);
    // A real (asynchronous) language detection has begun. Copy's
    // detectLanguage() is synchronous and never emits this. Terminates via
    // languageDetected() or any stateChanged() (abort/cancel safety net).
    void detectionStarted();
    void languageDetected(const Language &detectedLanguage, bool isTranslationContext = true);
    void engineChanged(int engineIndex);
};

#endif // ATRANSLATIONPROVIDER_H
