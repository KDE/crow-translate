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
    } else if (providerType == "LocalAiTranslationProvider") {
        return createLocalAiTranslationOptionsFromSettings();
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
        return createPiperTTSOptionsFromSettings();
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

    // None is stored until Mozhi is first used (GuiFrontend::run()); until
    // then the provider's own default instance stands.
    if (!settings.instance().isEmpty())
        options->setOption("instance", settings.instance());
    options->setOption("engine", static_cast<int>(settings.currentEngine()));
    options->setOption("apikey", settings.libreTranslateApiKey());
    options->setOption("direct", settings.libreTranslateDirect());

    return options;
}

std::unique_ptr<ProviderOptions> ProviderOptionsManager::createCopyTranslationOptionsFromSettings()
{
    return std::make_unique<ProviderOptions>();
}

std::unique_ptr<ProviderOptions> ProviderOptionsManager::createLocalAiTranslationOptionsFromSettings()
{
    const AppSettings settings;
    auto options = std::make_unique<ProviderOptions>();

    const QString active = settings.activeLocalProvider();
    const QString model = settings.localProviderModel(active);
    options->setOption("url", settings.localProviderUrl(active));
    options->setOption("model", model);
    options->setOption("prompt", settings.localAiPrompt(model));
    options->setOption("disable_thinking", settings.localAiDisableThinking(active));
    options->setOption("timeout", settings.localAiTimeout(active));
    options->setOption("api_key", settings.localProviderApiKey(active));
    options->setOption("is_anthropic", AppSettings::localProviderIsAnthropic(active));
    // Which language to assume when nothing was detected. The backend used to
    // read this out of AppSettings itself, which a loadable module cannot do -
    // and should not want to: it is a fact about the application's
    // preferences, not about the endpoint.
    options->setOption("fallback_language", settings.primaryLanguage().toCode());

    const QString detectProvider = settings.detectProvider();
    options->setOption("detect_url", settings.localProviderUrl(detectProvider));
    options->setOption("detect_model", settings.detectModel());
    options->setOption("detect_api_key", settings.localProviderApiKey(detectProvider));
    options->setOption("detect_is_anthropic", AppSettings::localProviderIsAnthropic(detectProvider));

    return options;
}

// TTS Provider Helper Methods
std::unique_ptr<ProviderOptions> ProviderOptionsManager::createMozhiTTSOptionsFromSettings()
{
    const AppSettings settings;
    auto options = std::make_unique<ProviderOptions>();

    if (!settings.instance().isEmpty())
        options->setOption("instance", settings.instance());
    options->setOption("engine", static_cast<int>(settings.currentEngine()));

    return options;
}

std::unique_ptr<ProviderOptions> ProviderOptionsManager::createQtTTSOptionsFromSettings()
{
    return std::make_unique<ProviderOptions>();
}

std::unique_ptr<ProviderOptions> ProviderOptionsManager::createPiperTTSOptionsFromSettings()
{
    auto options = std::make_unique<ProviderOptions>();

    // Set default speaker - this will be dynamically populated based on the current voice
    options->setOption("speaker", "default");

    // Where to look for voice models. The backend seeds this from the same
    // settings key when it starts, because discovery runs before any options
    // arrive; passing it here is what makes a change in the dialog take
    // effect without a restart.
    options->setOption("voices_path", AppSettings().piperVoicesPath());

    // Flag to trigger model reinitialization when voices path changes
    options->setOption("reinitializeModels", true);

    return options;
}

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
