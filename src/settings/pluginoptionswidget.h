/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef PLUGINOPTIONSWIDGET_H
#define PLUGINOPTIONSWIDGET_H

#include "plugininterfaces/iproviderplugin.h"

#include <QHash>
#include <QWidget>

class QFormLayout;
class QLineEdit;

// The settings a backend declares, drawn without knowing which backend it is.
//
// Every option a plugin marks user-visible gets a control here: a checkbox,
// a spin box, a line edit, or a line edit with a browse button for a
// directory. Nothing else in the dialog has to change for a backend it has
// never heard of to be configurable, which is the point - a plugin somebody
// else wrote is not going to have a page in this project's .ui file.
//
// Empty for every backend shipped here, and deliberately: their settings have
// hand-built pages that are better than anything a schema produces. The Mozhi
// instance is a combo box with a "find the fastest one" button behind it. What
// this replaced is the one option that had neither - Piper's voices
// directory, which was sixty lines of widget construction bolted onto the TTS
// page because there was nowhere else for it to go.
class PluginOptionsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PluginOptionsWidget(QWidget *parent = nullptr);

    // Rebuild for a backend. Hides itself when that backend declares nothing
    // to draw, so a dialog can host one of these unconditionally.
    void setBackend(PluginFamily family, const QString &id);

    // Read the controls back out, keyed by settings key.
    void load();
    void save() const;
    void restoreDefaults();
    // Hand over a value still being typed, for a dialog that is closing: a
    // line edit reports its value when editing finishes (see connectEdit()).
    void commitEdits();

signals:
    // A control changed while the dialog is still open. The dialog forwards
    // this so a live provider can follow it, as it always did for the two
    // options that had bespoke widgets.
    void optionChanged(const QString &key, const QVariant &value);

private:
    void clear();
    void addControl(const ProviderOptionSpec &spec);
    void connectEdit(QLineEdit *edit, const QString &key);
    void commitEdit(QLineEdit *edit, const QString &key);

    QFormLayout *m_layout = nullptr;
    // Needed for the fallback storage key of an option that names none.
    QString m_pluginId;
    QVector<ProviderOptionSpec> m_specs;
    QHash<QString, QWidget *> m_controls;
};

#endif // PLUGINOPTIONSWIDGET_H
