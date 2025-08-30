/*
 * SPDX-FileCopyrightText: 2025 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef COPYTRANSLATIONPROVIDER_H
#define COPYTRANSLATIONPROVIDER_H

#include "language.h"
#include "translator/atranslationprovider.h"

#include <QObject>

#include <memory>

class CopyTranslationProvider : public ATranslationProvider
{
    Q_OBJECT
    Q_DISABLE_COPY(CopyTranslationProvider)
public:
    explicit CopyTranslationProvider(QObject *parent = nullptr);

    QString getProviderType() const override;
    QVector<Language> supportedSourceLanguages() override;
    QVector<Language> supportedDestinationLanguages() override;
    bool supportsAutodetection() const override;
    Language detectLanguage(const QString &text) override;

    // Options system implementation
    void applyOptions(const ProviderOptions &options) override;
    std::unique_ptr<ProviderOptions> getDefaultOptions() const override;
    QStringList getAvailableOptions() const override;

    // UI requirements
    ProviderUIRequirements getUIRequirements() const override;

    // Settings integration
    void saveOptionToSettings(const QString &optionKey, const QVariant &value) override;

signals:

public slots:
    void translate(const QString &inputText, const Language &translationLanguage, const Language &sourceLanguage) override;
};

#endif // COPYTRANSLATIONPROVIDER_H
