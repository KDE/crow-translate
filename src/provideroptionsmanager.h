/*
 * SPDX-FileCopyrightText: 2025 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef PROVIDEROPTIONSMANAGER_H
#define PROVIDEROPTIONSMANAGER_H

#include "provideroptions.h"
#include "plugininterfaces/iproviderplugin.h"

#include <QObject>

class ATTSProvider;
class SettingsDialog;

// Filling a backend in from the settings file.
//
// This was six hand-written readers - one per backend - dispatched on the
// class name a provider answered getProviderType() with. Adding a backend
// meant adding a seventh and a branch to reach it, which was one of the three
// registries #738 is about.
//
// It reads the schema the backend declares instead: each option names the
// settings key it has always used, so nothing in anybody's configuration file
// moves, and a backend this manager has never heard of is filled in the same
// way as the ones shipped with it.
//
// Two backends still need help, and are named below. LocalAI and the
// vision-model OCR engine are configured per endpoint - five kinds of server,
// each with its own URL, model, key and timeout, plus a prompt stored per
// model - and no flat key describes a tree. Those are read here rather than
// in the backend because the rules attached to them are the application's:
// which endpoint is active, and which stored prompt is a superseded default
// that should be ignored.
class ProviderOptionsManager : public QObject
{
    Q_OBJECT

signals:
    void ttsProviderUIUpdateRequired();

public:
    explicit ProviderOptionsManager(QObject *parent = nullptr);

    // Everything the backend's schema describes, read from the settings file.
    // An option whose spec names no settings key is read from
    // Plugins/<id>/<key>, which is where a backend that predates none of this
    // gets its storage for free.
    ProviderOptions optionsFor(PluginFamily family, const QString &id) const;

    // Push one option into a live provider without going near the settings
    // file - a dialog that has not been accepted yet has nothing stored, and
    // the voice list still has to follow the instance the user just typed.
    // Ignored by a backend that does not declare the option.
    static void applyLiveOption(ATTSProvider *provider, const QString &key, const QVariant &value);

    // Follow a settings dialog's live changes into the current speech
    // backend, whichever one that is.
    void connectToSettingsDialog(SettingsDialog *settingsDialog, ATTSProvider *ttsProvider);

private:
    void augmentLocalAiTranslation(ProviderOptions &options) const;
    void augmentLlmOcr(ProviderOptions &options) const;
};

#endif // PROVIDEROPTIONSMANAGER_H
