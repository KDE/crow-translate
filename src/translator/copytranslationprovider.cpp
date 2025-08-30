/*
 * SPDX-FileCopyrightText: 2025 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "copytranslationprovider.h"

#include "provideroptions.h"
#include "translator/atranslationprovider.h"

CopyTranslationProvider::CopyTranslationProvider(QObject *parent)
    : ATranslationProvider{parent}
{
    emit stateChanged(state);
}

QString CopyTranslationProvider::getProviderType() const
{
    return QStringLiteral("CopyTranslationProvider");
}

QVector<Language> CopyTranslationProvider::supportedSourceLanguages()
{
    return {Language(QLocale::system())};
}

QVector<Language> CopyTranslationProvider::supportedDestinationLanguages()
{
    return {Language(QLocale::system())};
}

bool CopyTranslationProvider::supportsAutodetection() const
{
    return false;
}

Language CopyTranslationProvider::detectLanguage(const QString &text)
{
    Q_UNUSED(text);
    return Language(QLocale::system());
}

void CopyTranslationProvider::translate(const QString &inputText, const Language &translationLanguage, const Language &sourceLanguage)
{
    state = State::Processing;
    emit stateChanged(state);
    if (translationLanguage == sourceLanguage) {
        result = inputText;
        state = State::Processed;
        error = TranslationError::NoError;
    } else {
        state = State::Finished;
        error = TranslationError::UnsupportedDstLanguage;
    }
    emit stateChanged(state);
}

void CopyTranslationProvider::applyOptions(const ProviderOptions &options)
{
    Q_UNUSED(options);
    // Copy provider has no configurable options
}

std::unique_ptr<ProviderOptions> CopyTranslationProvider::getDefaultOptions() const
{
    return std::make_unique<ProviderOptions>();
}

QStringList CopyTranslationProvider::getAvailableOptions() const
{
    return {}; // No options available for copy provider
}

ProviderUIRequirements CopyTranslationProvider::getUIRequirements() const
{
    return {};
}

void CopyTranslationProvider::saveOptionToSettings(const QString &optionKey, const QVariant &value)
{
    Q_UNUSED(optionKey);
    Q_UNUSED(value);
    // Copy provider has no settings to save
}
