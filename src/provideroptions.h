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

// Provider UI requirements
struct ProviderUIRequirements {
    QStringList requiredUIElements; // List of UI elements needed (e.g., "engineComboBox")
    QStringList supportedSignals; // List of signals the provider emits
    QStringList supportedCapabilities; // List of capabilities (e.g., "languageDetection", "voiceSelection")
};

#endif // PROVIDEROPTIONS_H