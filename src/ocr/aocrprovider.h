/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef AOCRPROVIDER_H
#define AOCRPROVIDER_H

#include "crow_core_export.h"
#include "provideroptions.h"

#include <QImage>
#include <QObject>
#include <QString>
#include <QStringList>

class CROW_CORE_EXPORT AOcrProvider : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(AOcrProvider)

public:
    explicit AOcrProvider(QObject *parent = nullptr)
        : QObject(parent)
    {
    }
    ~AOcrProvider() override = default;

    virtual QString engineName() const = 0;
    virtual bool isConfigured() const = 0;
    virtual void recognize(const QImage &image, int dpi) = 0;
    virtual void cancel() = 0;

    // Configuration, the same way the other two families take it.
    //
    // This was a set of engine-specific setters that the session called by
    // name - setEndpoint(), setModel(), init(languages, path, parameters) -
    // which meant the session had to know which engine it was holding. An
    // engine that arrives as a module cannot be configured that way.
    virtual void applyOptions(const ProviderOptions &options) = 0;
    virtual QStringList getAvailableOptions() const = 0;

signals:
    // Recognition has actually begun (both engines are genuinely async), so
    // "recognizing" is an observable interval for the status strip.
    void started();
    void recognized(const QString &text);
    void failed(const QString &error);
    void canceled();

    // The options were applied and the engine still cannot run: Tesseract
    // has no data for the languages it was given. Whether that is worth
    // telling the user about is the host's question - a machine with no
    // language data installed did not choose anything - so this reports the
    // fact and says nothing about what should happen next.
    void configurationFailed(const QString &detail);
};

#endif // AOCRPROVIDER_H
