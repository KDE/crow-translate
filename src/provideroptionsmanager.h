/*
 * SPDX-FileCopyrightText: 2025 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef PROVIDEROPTIONSMANAGER_H
#define PROVIDEROPTIONSMANAGER_H

#include "provideroptions.h"

#include <QObject>

#include <memory>

class ATranslationProvider;
class ATTSProvider;
class AppSettings;
class SettingsDialog;

class ProviderOptionsManager : public QObject
{
    Q_OBJECT

signals:
    void ttsProviderUIUpdateRequired();

public:
    explicit ProviderOptionsManager(QObject *parent = nullptr);

    // Translation Provider Options
    std::unique_ptr<ProviderOptions> createTranslationOptionsFromSettings(ATranslationProvider *provider);
    void applySettingsToTranslationProvider(ATranslationProvider *provider);

    // TTS Provider Options
    std::unique_ptr<ProviderOptions> createTTSOptionsFromSettings(ATTSProvider *provider);
    void applySettingsToTTSProvider(ATTSProvider *provider);
    void handleTTSProviderSettingsChange(ATTSProvider *provider, const QString &settingKey);

    // Connect to settings changes automatically
    void connectToSettingsDialog(class SettingsDialog *settingsDialog, ATTSProvider *ttsProvider);

    // Backend validation
    static void validateTTSBackendAvailability();

private:
    // Helper methods for translation providers
    std::unique_ptr<ProviderOptions> createMozhiTranslationOptionsFromSettings();
    std::unique_ptr<ProviderOptions> createCopyTranslationOptionsFromSettings();

    // Helper methods for TTS providers
    std::unique_ptr<ProviderOptions> createMozhiTTSOptionsFromSettings();
    std::unique_ptr<ProviderOptions> createQtTTSOptionsFromSettings();
#ifdef WITH_PIPER_TTS
    std::unique_ptr<ProviderOptions> createPiperTTSOptionsFromSettings();
#endif
};

#endif // PROVIDEROPTIONSMANAGER_H