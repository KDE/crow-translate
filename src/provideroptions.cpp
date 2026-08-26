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
namespace
{
// Kept beside the enum it describes, so adding a capability is two adjacent
// lines rather than a table in another file that quietly falls behind.
struct CapabilityName {
    ProviderCapability capability;
    const char *name;
};

constexpr CapabilityName capabilityTable[] = {
    {ProviderCapability::LanguageDetection, "LanguageDetection"},
    {ProviderCapability::EngineSelection, "EngineSelection"},
    {ProviderCapability::ProviderSelection, "ProviderSelection"},
    {ProviderCapability::VoiceSelection, "VoiceSelection"},
    {ProviderCapability::SpeakerSelection, "SpeakerSelection"},
};
} // namespace

ProviderCapabilities capabilitiesFromNames(const QStringList &names)
{
    ProviderCapabilities capabilities = ProviderCapability::None;
    for (const QString &name : names) {
        for (const CapabilityName &known : capabilityTable) {
            if (name == QLatin1String(known.name)) {
                capabilities |= known.capability;
                break;
            }
        }
    }
    return capabilities;
}

QStringList capabilityNames(ProviderCapabilities capabilities)
{
    QStringList names;
    for (const CapabilityName &known : capabilityTable) {
        if (capabilities.testFlag(known.capability))
            names.append(QString::fromLatin1(known.name));
    }
    return names;
}
