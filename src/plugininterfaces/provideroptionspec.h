/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef PROVIDEROPTIONSPEC_H
#define PROVIDEROPTIONSPEC_H

#include <QMetaType>
#include <QString>
#include <QVariant>
#include <QVector>

// One knob a backend exposes, and where its value comes from.
//
// This is what replaces ProviderOptionsManager's six hand-written
// create*OptionsFromSettings() helpers. Each of them existed only to say
// which settings keys belong to which backend - knowledge the backend has
// and the manager had to be told. A backend that declares its own options
// can be filled in by a manager that has never heard of it, which is the
// whole point of loading one at runtime.
struct ProviderOptionSpec {
    // The name applyOptions() sees. Backends already agreed on these -
    // "instance", "engine", "speaker" - and nothing about them changes.
    QString key;

    // Where the host reads and writes the value.
    //
    // Named by the plugin rather than derived from its id, so that every
    // settings file 4.1.0 wrote keeps working: "Mozhi/Instance" stays
    // "Mozhi/Instance". A plugin that leaves this blank - which is every
    // plugin that did not exist before this scheme - gets
    // Plugins/<id>/<key>.
    QString settingsKey;

    QMetaType::Type type = QMetaType::QString;
    QVariant defaultValue;

    // For a settings dialog that builds its own controls. Blank means the
    // dialog draws this option's control by hand, as it does for the ones
    // that need a network probe or a per-model editor.
    QString displayName;
    QString description;

    // A value the host computes rather than the user choosing it: the
    // fallback language LocalAI detects into comes from the application's
    // own settings, not from a control anyone should be shown. Also false
    // for an option the settings dialog already draws better by hand - the
    // Mozhi instance is a combo box with a "find the fastest one" button
    // behind it, and no schema is going to produce that.
    bool userVisible = true;

    // What sort of control this wants. A plain value gets one appropriate to
    // its type; a directory gets a line edit with a browse button beside it,
    // which is the one shape a path needs and every backend that has one
    // would otherwise hand-build.
    enum class Control : quint8 {
        Value,
        Directory
    };
    Control control = Control::Value;
};

// Built by assignment rather than designated initializers: MSVC rejects
// those under C++17 even though clang accepts them, and this project builds
// on both.
inline ProviderOptionSpec makeOptionSpec(const QString &key,
                                         const QString &settingsKey,
                                         QMetaType::Type type,
                                         const QVariant &defaultValue,
                                         const QString &displayName = QString(),
                                         const QString &description = QString())
{
    ProviderOptionSpec spec;
    spec.key = key;
    spec.settingsKey = settingsKey;
    spec.type = type;
    spec.defaultValue = defaultValue;
    spec.displayName = displayName;
    spec.description = description;
    return spec;
}

// A directory the user picks, rendered with a browse button.
inline ProviderOptionSpec makeDirectoryOptionSpec(const QString &key,
                                                  const QString &settingsKey,
                                                  const QString &displayName,
                                                  const QString &description = QString())
{
    ProviderOptionSpec spec = makeOptionSpec(key, settingsKey, QMetaType::QByteArray, QByteArray(), displayName, description);
    spec.control = ProviderOptionSpec::Control::Directory;
    return spec;
}

// The same, for a value the user never sees.
inline ProviderOptionSpec makeDerivedOptionSpec(const QString &key, QMetaType::Type type, const QVariant &defaultValue)
{
    ProviderOptionSpec spec = makeOptionSpec(key, QString(), type, defaultValue);
    spec.userVisible = false;
    return spec;
}

#endif // PROVIDEROPTIONSPEC_H
