/*
 * SPDX-FileCopyrightText: 2025 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "voice.h"

#include <utility>

Voice::Voice() = default;

Voice::Voice(QString name, const Language &language, QVariantMap data)
    : m_name(std::move(name))
    , m_language(language)
    , m_data(std::move(data))
{
}

Voice::Voice(const QVoice &qvoice)
    : m_name(qvoice.name())
    , m_language(Language(qvoice.locale()))
{
    m_data["qvoice"] = QVariant::fromValue(qvoice);
}

Voice::Voice(const Voice &other) = default;

Voice &Voice::operator=(const Voice &other)
{
    if (this != &other) {
        m_name = other.m_name;
        m_language = other.m_language;
        m_data = other.m_data;
    }
    return *this;
}

QString Voice::name() const
{
    return m_name;
}

Language Voice::language() const
{
    return m_language;
}

QVariantMap Voice::data() const
{
    return m_data;
}

QString Voice::modelPath() const
{
    return m_data.value("modelPath").toString();
}

void Voice::setModelPath(const QString &path)
{
    m_data["modelPath"] = path;
}

bool Voice::isValid() const
{
    return !m_name.isEmpty();
}

QVoice Voice::toQVoice() const
{
    if (m_data.contains("qvoice")) {
        return m_data.value("qvoice").value<QVoice>();
    }
    return {};
}

bool Voice::operator==(const Voice &other) const
{
    return m_name == other.m_name && m_language == other.m_language;
}

bool Voice::operator!=(const Voice &other) const
{
    return !(*this == other);
}
