/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef AOCRPROVIDER_H
#define AOCRPROVIDER_H

#include <QImage>
#include <QObject>
#include <QString>

class AOcrProvider : public QObject
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

signals:
    // Recognition has actually begun (both engines are genuinely async), so
    // "recognizing" is an observable interval for the status strip.
    void started();
    void recognized(const QString &text);
    void failed(const QString &error);
    void canceled();
};

#endif // AOCRPROVIDER_H
