/*
 * SPDX-FileCopyrightText: 2025 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "provideroptions.h"

void ProviderOptions::setOption(const QString &key, const QVariant &value)
{
    m_options[key] = value;
}

QVariant ProviderOptions::getOption(const QString &key, const QVariant &defaultValue) const
{
    return m_options.value(key, defaultValue);
}

bool ProviderOptions::hasOption(const QString &key) const
{
    return m_options.contains(key);
}

QVariantMap ProviderOptions::getAllOptions() const
{
    return m_options;
}

void ProviderOptions::clearOptions()
{
    m_options.clear();
}