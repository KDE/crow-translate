/*
 * SPDX-FileCopyrightText: 2025 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "provideroptionsmanager.h"

#include "onlinetranslator.h"
#include "settings/appsettings.h"
#include "settings/settingsdialog.h"
#include "translator/atranslationprovider.h"
#include "tts/attsprovider.h"

ProviderOptionsManager::ProviderOptionsManager(QObject *parent)
    : QObject(parent)
{
}

// Translation Provider Methods
std::unique_ptr<ProviderOptions> ProviderOptionsManager::createTranslationOptionsFromSettings(ATranslationProvider *provider)
{
    if (provider == nullptr) {
        return nullptr;
    }

    const QString providerType = provider->getProviderType();
    if (providerType == "MozhiTranslationProvider") {
        return createMozhiTranslationOptionsFromSettings();
    } else if (providerType == "CopyTranslationProvider") {
        return createCopyTranslationOptionsFromSettings();
    }

    return nullptr;
}

void ProviderOptionsManager::applySettingsToTranslationProvider(ATranslationProvider *provider)
{
    if (provider == nullptr) {
        return;
    }

    auto options = createTranslationOptionsFromSettings(provider);
    if (options) {
        provider->applyOptions(*options);
    }
}

// TTS Provider Methods
std::unique_ptr<ProviderOptions> ProviderOptionsManager::createTTSOptionsFromSettings(ATTSProvider *provider)
{
    if (provider == nullptr) {
        return nullptr;
    }

    const QString providerType = provider->getProviderType();
    if (providerType == "MozhiTTSProvider") {
        return createMozhiTTSOptionsFromSettings();
    } else if (providerType == "QtTTSProvider") {
        // TODO: Implement QtTTSProvider options - for now use Mozhi options
        return createMozhiTTSOptionsFromSettings();
    } else if (providerType == "PiperTTSProvider") {
#ifdef WITH_PIPER_TTS
        return createPiperTTSOptionsFromSettings();
#else
        return createMozhiTTSOptionsFromSettings();
#endif
    }

    return nullptr;
}

void ProviderOptionsManager::applySettingsToTTSProvider(ATTSProvider *provider)
{
    if (provider == nullptr) {
        return;
    }

    auto options = createTTSOptionsFromSettings(provider);
    if (options) {
        provider->applyOptions(*options);
    }
}

// Translation Provider Helper Methods
std::unique_ptr<ProviderOptions> ProviderOptionsManager::createMozhiTranslationOptionsFromSettings()
{
    const AppSettings settings;
    auto options = std::make_unique<ProviderOptions>();

    options->setOption("instance", settings.instance());
    options->setOption("engine", static_cast<int>(settings.currentEngine()));

    return options;
}

std::unique_ptr<ProviderOptions> ProviderOptionsManager::createCopyTranslationOptionsFromSettings()
{
    return std::make_unique<ProviderOptions>();
}

// TTS Provider Helper Methods
std::unique_ptr<ProviderOptions> ProviderOptionsManager::createMozhiTTSOptionsFromSettings()
{
    const AppSettings settings;
    auto options = std::make_unique<ProviderOptions>();

    options->setOption("instance", settings.instance());
    options->setOption("engine", static_cast<int>(settings.currentEngine()));

    return options;
}

std::unique_ptr<ProviderOptions> ProviderOptionsManager::createQtTTSOptionsFromSettings()
{
    return std::make_unique<ProviderOptions>();
}

#ifdef WITH_PIPER_TTS
std::unique_ptr<ProviderOptions> ProviderOptionsManager::createPiperTTSOptionsFromSettings()
{
    auto options = std::make_unique<ProviderOptions>();

    // Set default speaker - this will be dynamically populated based on the current voice
    options->setOption("speaker", "default");

    // Flag to trigger model reinitialization when voices path changes
    options->setOption("reinitializeModels", true);

    return options;
}
#endif

void ProviderOptionsManager::handleTTSProviderSettingsChange(ATTSProvider *provider, const QString &settingKey)
{
    if (!provider) {
        return;
    }

    const QString providerType = provider->getProviderType();

    // Handle settings that require reapplying options
    if (providerType == "MozhiTTSProvider" && settingKey == "Mozhi/Instance") {
        qDebug() << "ProviderOptionsManager: Reapplying settings for" << providerType << "after" << settingKey << "change";
        applySettingsToTTSProvider(provider);

        // Emit signal to update UI elements that depend on provider state
        emit ttsProviderUIUpdateRequired();
    }
}

void ProviderOptionsManager::connectToSettingsDialog(SettingsDialog *settingsDialog, ATTSProvider *ttsProvider)
{
    if (!settingsDialog || !ttsProvider) {
        return;
    }

    // Connect to provider-specific setting changes
    connect(settingsDialog, &SettingsDialog::piperVoicesPathChanged, this, [this, ttsProvider](const QString &) {
        handleTTSProviderSettingsChange(ttsProvider, "TTS/PiperVoicesPath");
    });

    connect(settingsDialog, &SettingsDialog::mozhiInstanceChanged, this, [this, ttsProvider](const QString &newInstance) {
        qDebug() << "ProviderOptionsManager: Applying new Mozhi instance immediately:" << newInstance;
        // Apply the new instance directly instead of reading from old settings
        if (ttsProvider && ttsProvider->getProviderType() == "MozhiTTSProvider") {
            auto options = std::make_unique<ProviderOptions>();
            options->setOption("instance", newInstance);
            ttsProvider->applyOptions(*options);
        }
    });
}

void ProviderOptionsManager::validateTTSBackendAvailability()
{
    AppSettings settings;
    auto chosenTTSBackend = settings.ttsProviderBackend();

    // Validate TTS backend availability - reset Piper to None if dependencies unavailable
#ifndef WITH_PIPER_TTS
    if (chosenTTSBackend == ATTSProvider::ProviderBackend::Piper) {
        chosenTTSBackend = ATTSProvider::ProviderBackend::None;
        settings.setTTSProviderBackend(chosenTTSBackend);
    }
#endif
}
