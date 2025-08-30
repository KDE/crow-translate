/*
 * SPDX-FileCopyrightText: 2025 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef VOICE_H
#define VOICE_H

#include "language.h"

#include <QLocale>
#include <QString>
#include <QVariant>
#include <QVariantMap>
#include <QVoice>

class Voice
{
public:
    Voice();
    Voice(QString name, const Language &language, QVariantMap data = QVariantMap());
    Voice(const QVoice &qvoice);
    Voice(const Voice &other);
    Voice &operator=(const Voice &other);

    QString name() const;
    Language language() const;
    QVariantMap data() const;

    // Convenience methods for common data access
    QString modelPath() const;
    void setModelPath(const QString &path);

    bool isValid() const;

    QVoice toQVoice() const;

    bool operator==(const Voice &other) const;
    bool operator!=(const Voice &other) const;

private:
    QString m_name;
    Language m_language;
    QVariantMap m_data;
};

#endif // VOICE_H