/*
 * SPDX-FileCopyrightText: 2025 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "provideroptionsmanager.h"

#include "core/pluginmanager.h"
#include "core/settingsstore.h"
#include "settings/appsettings.h"
#include "settings/settingsdialog.h"
#include "tts/attsprovider.h"

#include <QSettings>

#include <memory>

namespace
{

// Storage for a backend that names no key of its own. Never used by the ones
// shipped here - every one of those names the key it already wrote - so this
// is what a third-party backend gets without having to invent anything.
QString fallbackKey(const QString &pluginId, const QString &optionKey)
{
    return QStringLiteral("Plugins/") + pluginId + QLatin1Char('/') + optionKey;
}

// A whole settings group as a map. Tesseract's parameters are one: an
// arbitrary set of engine variables the user edits in a table, which is a
// group rather than a value and so cannot be a key.
QVariantMap readGroup(QSettings &settings, const QString &group)
{
    QVariantMap values;
    settings.beginGroup(group);
    const QStringList keys = settings.childKeys();
    for (const QString &key : keys)
        values.insert(key, settings.value(key));
    settings.endGroup();
    return values;
}

} // namespace

ProviderOptionsManager::ProviderOptionsManager(QObject *parent)
    : QObject(parent)
{
}

ProviderOptions ProviderOptionsManager::optionsFor(PluginFamily family, const QString &id) const
{
    ProviderOptions options;

    const std::optional<PluginInfo> described = PluginManager::instance()->describe(family, id);
    if (!described.has_value())
        return options;

    const std::unique_ptr<QSettings> settings(CrowSettings::open());
    for (const ProviderOptionSpec &spec : described->options) {
        const QString key = spec.settingsKey.isEmpty() ? fallbackKey(id, spec.key) : spec.settingsKey;

        if (spec.type == QMetaType::QVariantMap) {
            options.setOption(spec.key, readGroup(*settings, key));
            continue;
        }

        options.setOption(spec.key, settings->value(key, spec.defaultValue));
    }

    // See the header: two backends are configured per endpoint, and the rules
    // for reading that tree belong to the application rather than to them.
    if (family == PluginFamily::Translation && id == QLatin1String("localai"))
        augmentLocalAiTranslation(options);
    else if (family == PluginFamily::Ocr && id == QLatin1String("llm"))
        augmentLlmOcr(options);

    return options;
}

void ProviderOptionsManager::augmentLocalAiTranslation(ProviderOptions &options) const
{
    const AppSettings settings;

    const QString active = settings.activeLocalProvider();
    const QString model = settings.localProviderModel(active);
    options.setOption(QStringLiteral("url"), settings.localProviderUrl(active));
    options.setOption(QStringLiteral("model"), model);
    // Not the raw stored value: localAiPrompt() also rejects the default this
    // one replaced, which was persisted on every keystroke by a dialog that
    // no longer does that.
    options.setOption(QStringLiteral("prompt"), settings.localAiPrompt(model));
    options.setOption(QStringLiteral("disable_thinking"), settings.localAiDisableThinking(active));
    options.setOption(QStringLiteral("timeout"), settings.localAiTimeout(active));
    options.setOption(QStringLiteral("api_key"), settings.localProviderApiKey(active));
    options.setOption(QStringLiteral("is_anthropic"), AppSettings::localProviderIsAnthropic(active));
    // Which language to assume when nothing was detected. A fact about the
    // application's preferences, not about the endpoint.
    options.setOption(QStringLiteral("fallback_language"), settings.primaryLanguage().toCode());

    const QString detectProvider = settings.detectProvider();
    options.setOption(QStringLiteral("detect_url"), settings.localProviderUrl(detectProvider));
    options.setOption(QStringLiteral("detect_model"), settings.detectModel());
    options.setOption(QStringLiteral("detect_api_key"), settings.localProviderApiKey(detectProvider));
    options.setOption(QStringLiteral("detect_is_anthropic"), AppSettings::localProviderIsAnthropic(detectProvider));
}

void ProviderOptionsManager::augmentLlmOcr(ProviderOptions &options) const
{
    const AppSettings settings;

    // Its own group, deliberately: the endpoint that transcribes your
    // screenshots is not the one that translates your text, and changing one
    // must never move the other.
    const QString endpoint = settings.ocrLlmProvider();
    const QString model = settings.ocrLlmModel(endpoint);
    options.setOption(QStringLiteral("url"), settings.ocrLlmUrl(endpoint));
    options.setOption(QStringLiteral("is_anthropic"), AppSettings::localProviderIsAnthropic(endpoint));
    options.setOption(QStringLiteral("api_key"), settings.ocrLlmApiKey(endpoint));
    options.setOption(QStringLiteral("model"), model);
    options.setOption(QStringLiteral("timeout"), settings.ocrLlmTimeout(endpoint));
    options.setOption(QStringLiteral("prompt"), settings.ocrLlmPrompt(model));
    options.setOption(QStringLiteral("disable_thinking"), settings.localAiDisableThinking(endpoint));
}

void ProviderOptionsManager::applyLiveOption(ATTSProvider *provider, const QString &key, const QVariant &value)
{
    if (provider == nullptr || !provider->getAvailableOptions().contains(key)) {
        return;
    }
    ProviderOptions options;
    options.setOption(key, value);
    provider->applyOptions(options);
}

void ProviderOptionsManager::connectToSettingsDialog(SettingsDialog *settingsDialog, ATTSProvider *ttsProvider)
{
    if (settingsDialog == nullptr || ttsProvider == nullptr) {
        return;
    }

    // Both of these used to check which backend they were holding - one
    // branch for "MozhiTTSProvider" and a settings key spelled out by name,
    // another for Piper's voices path. The option either exists on the
    // current backend or it does not, and that is the whole question.
    connect(settingsDialog, &SettingsDialog::piperVoicesPathChanged, this, [this, ttsProvider](const QString &path) {
        applyLiveOption(ttsProvider, QStringLiteral("voices_path"), path.toLocal8Bit());
        applyLiveOption(ttsProvider, QStringLiteral("reinitializeModels"), true);
        Q_EMIT ttsProviderUIUpdateRequired();
    });
    connect(settingsDialog, &SettingsDialog::mozhiInstanceChanged, this, [this, ttsProvider](const QString &newInstance) {
        applyLiveOption(ttsProvider, QStringLiteral("instance"), newInstance);
        Q_EMIT ttsProviderUIUpdateRequired();
    });
}
