/*
 * SPDX-FileCopyrightText: 2025 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef PROVIDEROPTIONS_H
#define PROVIDEROPTIONS_H

#include <QIcon>
#include <QObject>
#include <QString>
#include <QVariant>
#include <QVariantMap>
#include <QVector>

class ProviderOptions
{
public:
    ProviderOptions() = default;
    virtual ~ProviderOptions() = default;

    // Set an option value
    void setOption(const QString &key, const QVariant &value);

    // Get an option value
    QVariant getOption(const QString &key, const QVariant &defaultValue = QVariant()) const;

    // Check if an option exists
    bool hasOption(const QString &key) const;

    // Get all options
    QVariantMap getAllOptions() const;

    // Clear all options
    void clearOptions();

private:
    QVariantMap m_options;
};

// Information about an option choice (e.g., different engines)
struct ProviderOptionItem {
    QString name; // Display name (e.g., "Google", "Yandex")
    QString description; // Optional description
    QIcon icon; // Optional icon
    QVariant value; // Internal value (e.g., enum value as int)
    bool isDefault; // Whether this is the default option

    ProviderOptionItem(const QString &name, const QVariant &value, bool isDefault = false)
        : name(name)
        , value(value)
        , isDefault(isDefault)
    {
    }

    ProviderOptionItem(const QString &name, const QVariant &value, const QIcon &icon, bool isDefault = false)
        : name(name)
        , icon(icon)
        , value(value)
        , isDefault(isDefault)
    {
    }

    ProviderOptionItem(const QString &name, const QString &description, const QVariant &value, const QIcon &icon, bool isDefault = false)
        : name(name)
        , description(description)
        , icon(icon)
        , value(value)
        , isDefault(isDefault)
    {
    }
};

// Information about a configurable option
struct ProviderOptionInfo {
    QString optionKey; // Option key (e.g., "engine", "instance")
    QString displayName; // Human-readable name (e.g., "Engine", "Instance URL")
    QString description; // Optional description
    QVector<ProviderOptionItem> items; // Available options

    ProviderOptionInfo(const QString &optionKey, const QString &displayName)
        : optionKey(optionKey)
        , displayName(displayName)
    {
    }

    ProviderOptionInfo(const QString &optionKey, const QString &displayName, const QString &description)
        : optionKey(optionKey)
        , displayName(displayName)
        , description(description)
    {
    }

    // Find item by value
    ProviderOptionItem findItemByValue(const QVariant &value) const
    {
        for (const auto &item : items) {
            if (item.value == value) {
                return item;
            }
        }
        return ProviderOptionItem("", QVariant());
    }

    // Get default item
    ProviderOptionItem getDefaultItem() const
    {
        for (const auto &item : items) {
            if (item.isDefault) {
                return item;
            }
        }
        return items.isEmpty() ? ProviderOptionItem("", QVariant()) : items.first();
    }
};

// What a backend can do.
//
// This replaced a struct of QStringLists. Two of its three fields were the
// problem: requiredUIElements named *widgets* - "engineComboBox",
// "sourceVoiceComboBox" - so a backend had to know the layout of one
// particular window in order to be used at all, and any frontend laid out
// differently was stuck with a list of controls it does not have. The names
// were also fully derivable from the capabilities beside them, so they said
// nothing extra; the derivation belongs to whichever frontend owns the
// widgets, and now lives there.
//
// The third field, supportedSignals, was assigned by two backends and read by
// nobody, and is gone.
//
// Flags rather than strings so a typo is a compile error. "voiceSelection"
// silently matched nothing.
enum class ProviderCapability : uint16_t {
    None = 0,
    // Can work out the source language on its own.
    LanguageDetection = 1u << 0,
    // Offers a choice of engines behind one backend, the way Mozhi fronts
    // Google, Yandex and the rest.
    EngineSelection = 1u << 1,
    // Offers a choice of upstream providers, which is LocalAI's list of
    // configured endpoints rather than a fixed set of engines.
    ProviderSelection = 1u << 2,
    // Has more than one voice to say things in.
    VoiceSelection = 1u << 3,
    // Has named speakers within a voice, as Piper's multi-speaker models do.
    SpeakerSelection = 1u << 4,
};
Q_DECLARE_FLAGS(ProviderCapabilities, ProviderCapability)
Q_DECLARE_OPERATORS_FOR_FLAGS(ProviderCapabilities)

#endif // PROVIDEROPTIONS_H