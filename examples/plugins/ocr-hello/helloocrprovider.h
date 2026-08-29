/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef HELLOOCRPROVIDER_H
#define HELLOOCRPROVIDER_H

#include "ocr/aocrprovider.h"

#include <QObject>
#include <QString>

// A recognition engine that describes the image rather than reading it.
//
// The narrowest of the three interfaces: hand it an image, get text back or a
// reason why not. Everything else about how the picture arrived - a screen
// grab, a dropped file, --image on the command line - is settled before this
// is called and none of a backend's business.
class HelloOcrProvider : public AOcrProvider
{
    Q_OBJECT

public:
    explicit HelloOcrProvider(QObject *parent = nullptr);

    QString engineName() const override;
    bool isConfigured() const override;
    void recognize(const QImage &image, int dpi) override;
    void cancel() override;

    void applyOptions(const ProviderOptions &options) override;
    QStringList getAvailableOptions() const override;

private:
    QString m_label;
    bool m_canceled = false;
};

#endif // HELLOOCRPROVIDER_H
