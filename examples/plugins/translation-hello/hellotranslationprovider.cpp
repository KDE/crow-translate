/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "hellotranslationprovider.h"

#include "provideroptions.h"

namespace
{
// Repeated in hellotranslationplugin.cpp, which declares the same option to
// the host. The two have to agree - a declared option the provider does not
// accept, or an accepted one nobody declares, is what test_pluginmanager's
// conformance case fails a backend for.
const QString prefixOption = QStringLiteral("prefix");
const QString defaultPrefix = QStringLiteral("[hello] ");
}

HelloTranslationProvider::HelloTranslationProvider(QObject *parent)
    : ATranslationProvider{parent}
    , m_prefix{defaultPrefix}
{
    emit stateChanged(state);
}

// Free text, and the frontend shows it as-is. Conventionally the class name.
QString HelloTranslationProvider::getProviderType() const
{
    return QStringLiteral("HelloTranslationProvider");
}

// What --codes prints and what the language combo boxes are filled from. A
// real backend answers with whatever its service supports; returning an empty
// list is the one thing that is wrong, and the contract suite says so.
QVector<Language> HelloTranslationProvider::supportedSourceLanguages()
{
    return {Language(QLocale::English), Language(QLocale::German), Language(QLocale::system())};
}

QVector<Language> HelloTranslationProvider::supportedDestinationLanguages()
{
    return supportedSourceLanguages();
}

bool HelloTranslationProvider::supportsAutodetection() const
{
    return false;
}

Language HelloTranslationProvider::detectLanguage(const QString &text)
{
    Q_UNUSED(text)
    // Answering anything else while supportsAutodetection() is false would be
    // a claim the GUI's auto button takes at face value.
    return Language(QLocale::system());
}

void HelloTranslationProvider::translate(const QString &inputText, const Language &translationLanguage, const Language &sourceLanguage)
{
    // Record the pair this ran between before anything else. It is the only
    // fact the rest of the application has to work from when it decides which
    // voice to speak the result in.
    this->sourceLanguage = sourceLanguage;
    this->translationLanguage = translationLanguage;

    state = State::Processing;
    emit stateChanged(state);

    // Plain text. A backend fills in fields; turning them into a document is
    // the frontend's job, and a backend that returns markup is one the CLI
    // will read out with the tags in it.
    result.clear();
    result.translation = m_prefix + inputText;

    error = TranslationError::NoError;
    state = State::Processed;
    emit stateChanged(state);

    // Synchronous, which a network-backed provider will not be. The states are
    // the contract: Processing when work starts, then Processed with a result
    // or Finished with an error set. Nothing waits on translate() returning.
}

// Filled in by the host from the settings file, using the schema the plugin
// declares. Called again whenever the user changes one, so it has to be
// re-entrant and cheap.
void HelloTranslationProvider::applyOptions(const ProviderOptions &options)
{
    if (options.hasOption(prefixOption))
        m_prefix = options.getOption(prefixOption).toString();
}

std::unique_ptr<ProviderOptions> HelloTranslationProvider::getDefaultOptions() const
{
    auto options = std::make_unique<ProviderOptions>();
    options->setOption(prefixOption, defaultPrefix);
    return options;
}

QStringList HelloTranslationProvider::getAvailableOptions() const
{
    return {prefixOption};
}

// What the frontends may offer for this backend. Flags rather than widget
// names: which control an engine choice deserves is the frontend's question,
// and a backend that answered it could only ever be right about one window.
ProviderCapabilities HelloTranslationProvider::capabilities() const
{
    return ProviderCapability::None;
}
