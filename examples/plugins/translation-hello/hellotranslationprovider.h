/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef HELLOTRANSLATIONPROVIDER_H
#define HELLOTRANSLATIONPROVIDER_H

// The include path is the one an in-tree backend writes, because the installed
// headers mirror the source tree. A backend can be moved out of Crow's tree,
// or copied into it, without touching a single #include.
#include "translator/atranslationprovider.h"

#include <QObject>

#include <memory>

// The backend itself: what the application ends up holding once it has asked
// the plugin for one.
//
// Note what is *not* here. No Q_PLUGIN_METADATA, no interface, no factory -
// this is a plain ATranslationProvider, exactly what a backend compiled into
// the application would be. The plugin half lives next door in
// hellotranslationplugin.h, and the split is worth keeping: it is what lets a
// test link this class directly and drive it without loading a module.
class HelloTranslationProvider : public ATranslationProvider
{
    Q_OBJECT

public:
    explicit HelloTranslationProvider(QObject *parent = nullptr);

    QString getProviderType() const override;

    QVector<Language> supportedSourceLanguages() override;
    QVector<Language> supportedDestinationLanguages() override;
    bool supportsAutodetection() const override;
    Language detectLanguage(const QString &text) override;

    void applyOptions(const ProviderOptions &options) override;
    std::unique_ptr<ProviderOptions> getDefaultOptions() const override;
    QStringList getAvailableOptions() const override;

    ProviderCapabilities capabilities() const override;

public slots:
    void translate(const QString &inputText, const Language &translationLanguage, const Language &sourceLanguage) override;

private:
    QString m_prefix;
};

#endif // HELLOTRANSLATIONPROVIDER_H
