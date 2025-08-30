/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 * SPDX-FileCopyrightText: 2025 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef MOZHITRANSLATIONPROVIDER_H
#define MOZHITRANSLATIONPROVIDER_H

#include "language.h"
#include "onlinetranslator.h"
#include "translator/atranslationprovider.h"

#include <QObject>

#include <memory>

class MozhiTranslationProvider : public ATranslationProvider
{
    Q_OBJECT
    Q_DISABLE_COPY(MozhiTranslationProvider)

public:
    explicit MozhiTranslationProvider(QObject *parent = nullptr);
    ~MozhiTranslationProvider() override;

    QString getProviderType() const override;

    // ATranslationProvider interface
    QVector<Language> supportedSourceLanguages() override;
    QVector<Language> supportedDestinationLanguages() override;
    bool supportsAutodetection() const override;
    Language detectLanguage(const QString &text) override;
    void abort() override;
    void reset() override;

    // Options system implementation
    void applyOptions(const ProviderOptions &options) override;
    std::unique_ptr<ProviderOptions> getDefaultOptions() const override;
    QStringList getAvailableOptions() const override;

    // UI requirements
    ProviderUIRequirements getUIRequirements() const override;

    // Settings integration
    void saveOptionToSettings(const QString &optionKey, const QVariant &value) override;

    // Set the Mozhi instance URL
    void setInstance(const QString &instanceUrl);
    QString instance() const;

    // Set the translation engine to use
    void setEngine(OnlineTranslator::Engine engine);
    OnlineTranslator::Engine engine() const;

public slots:
    void translate(const QString &inputText, const Language &translationLanguage, const Language &sourceLanguage) override;

signals:

private slots:
    void onTranslationFinished();

private:
    // Format translation data similar to TranslationEdit::parseTranslationData
    QString formatTranslationData(OnlineTranslator *translator);
    // Helper methods to convert between Language and OnlineTranslator::Language
    static OnlineTranslator::Language toOnlineTranslatorLanguage(const Language &language, OnlineTranslator::Engine engine);
    static Language fromOnlineTranslatorLanguage(OnlineTranslator::Language lang, OnlineTranslator::Engine engine);

    // Get all supported languages for the current engine
    QVector<Language> getAllSupportedLanguages() const;

    // Register OnlineTranslator languages that aren't in Language as custom languages
    void registerCustomLanguages();

    OnlineTranslator *m_translator;
    OnlineTranslator::Engine m_engine;
    QString m_instanceUrl;
    bool m_isDetecting;
};

#endif // MOZHITRANSLATIONPROVIDER_H
