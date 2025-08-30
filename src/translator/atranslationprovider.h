/*
 * SPDX-FileCopyrightText: 2025 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef ATRANSLATIONPROVIDER_H
#define ATRANSLATIONPROVIDER_H

#include "language.h"

#include <QList>
#include <QLocale>
#include <QObject>

#include <memory>

class ProviderOptions;
struct ProviderUIRequirements;

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
        Mozhi
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

    // Options system
    virtual void applyOptions(const ProviderOptions &options) = 0;
    virtual std::unique_ptr<ProviderOptions> getDefaultOptions() const = 0;
    virtual QStringList getAvailableOptions() const = 0;

    // UI requirements
    virtual ProviderUIRequirements getUIRequirements() const = 0;

    // Settings integration (type-safe)
    virtual void saveOptionToSettings(const QString &optionKey, const QVariant &value) = 0;

    TranslationError error = TranslationError::NoError;
    Language sourceLanguage = Language::autoLanguage();
    Language translationLanguage = Language::autoLanguage();
    QString result;

public slots:
    virtual void translate(const QString &inputText, const Language &translationLang, const Language &sourceLang) = 0;

private:
protected:
    State state = State::Ready;
    QString errorString;
    explicit ATranslationProvider(QObject *parent = nullptr);

signals:
    void stateChanged(State newState);
    void languageDetected(const Language &detectedLanguage, bool isTranslationContext = true);
    void engineChanged(int engineIndex);
};

#endif // ATRANSLATIONPROVIDER_H
