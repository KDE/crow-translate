/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "settings/pluginoptionswidget.h"

#include "core/pluginmanager.h"
#include "core/settingsstore.h"

#include <QCheckBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QSettings>
#include <QSpinBox>
#include <QToolButton>

#include <memory>

namespace
{

QString settingsKeyFor(const QString &pluginId, const ProviderOptionSpec &spec)
{
    if (!spec.settingsKey.isEmpty())
        return spec.settingsKey;
    return QStringLiteral("Plugins/") + pluginId + QLatin1Char('/') + spec.key;
}

} // namespace

PluginOptionsWidget::PluginOptionsWidget(QWidget *parent)
    : QWidget(parent)
    , m_layout(new QFormLayout(this))
{
    m_layout->setContentsMargins(0, 0, 0, 0);
    hide();
}

void PluginOptionsWidget::clear()
{
    while (m_layout->count() > 0) {
        QLayoutItem *item = m_layout->takeAt(0);
        delete item->widget();
        delete item;
    }
    m_controls.clear();
    m_specs.clear();
    m_pluginId.clear();
}

void PluginOptionsWidget::setBackend(PluginFamily family, const QString &id)
{
    clear();
    m_pluginId = id;

    const std::optional<PluginInfo> described = PluginManager::instance()->describe(family, id);
    if (described.has_value()) {
        for (const ProviderOptionSpec &spec : described->options) {
            if (!spec.userVisible || spec.displayName.isEmpty())
                continue;
            m_specs.append(spec);
            addControl(spec);
        }
    }

    setVisible(!m_specs.isEmpty());
    load();
}

void PluginOptionsWidget::addControl(const ProviderOptionSpec &spec)
{
    QWidget *control = nullptr;

    switch (spec.control) {
    case ProviderOptionSpec::Control::Directory: {
        auto *row = new QWidget(this);
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        auto *edit = new QLineEdit(row);
        edit->setObjectName(spec.key);
        auto *browse = new QToolButton(row);
        browse->setText(QStringLiteral("..."));
        browse->setIcon(QIcon::fromTheme(QStringLiteral("folder")));
        browse->setToolTip(tr("Select a directory"));
        rowLayout->addWidget(edit);
        rowLayout->addWidget(browse);
        connect(browse, &QToolButton::clicked, this, [this, edit, spec]() {
            const QString directory = QFileDialog::getExistingDirectory(this, spec.displayName, edit->text());
            if (!directory.isEmpty() && directory != edit->text()) {
                edit->setText(directory);
                Q_EMIT optionChanged(spec.key, directory);
            }
        });
        connectEdit(edit, spec.key);
        control = row;
        m_controls.insert(spec.key, edit);
        break;
    }
    case ProviderOptionSpec::Control::Value:
        if (spec.type == QMetaType::Bool) {
            auto *box = new QCheckBox(this);
            box->setObjectName(spec.key);
            connect(box, &QCheckBox::toggled, this, [this, spec](bool value) {
                Q_EMIT optionChanged(spec.key, value);
            });
            control = box;
            m_controls.insert(spec.key, box);
        } else if (spec.type == QMetaType::Int) {
            auto *spin = new QSpinBox(this);
            spin->setObjectName(spec.key);
            spin->setRange(0, 100000);
            connect(spin, &QSpinBox::valueChanged, this, [this, spec](int value) {
                Q_EMIT optionChanged(spec.key, value);
            });
            control = spin;
            m_controls.insert(spec.key, spin);
        } else {
            auto *edit = new QLineEdit(this);
            edit->setObjectName(spec.key);
            connectEdit(edit, spec.key);
            control = edit;
            m_controls.insert(spec.key, edit);
        }
        break;
    }

    if (control != nullptr) {
        control->setToolTip(spec.description);
        m_layout->addRow(spec.displayName, control);
    }
}

// A typed value reaches the backend once the user is done with it, not per
// keystroke - as Piper's voices path did before this widget drew it
// (editingFinished). Every change is applied to the live backend at once, and
// for a path that is a rescan: typing C:\Voices rescanned C:\, then C:\V and so
// on, each on the GUI thread (C:\ alone took 13 s), and each that found nothing
// raised a "no voice models" box - thirty for one path, and they blocked the
// dialog's OK.
void PluginOptionsWidget::connectEdit(QLineEdit *edit, const QString &key)
{
    connect(edit, &QLineEdit::editingFinished, this, [this, edit, key]() {
        commitEdit(edit, key);
    });
}

void PluginOptionsWidget::commitEdit(QLineEdit *edit, const QString &key)
{
    if (!edit->isModified())
        return;
    edit->setModified(false);
    Q_EMIT optionChanged(key, edit->text());
}

void PluginOptionsWidget::commitEdits()
{
    for (auto control = m_controls.cbegin(); control != m_controls.cend(); ++control) {
        if (auto *edit = qobject_cast<QLineEdit *>(control.value()))
            commitEdit(edit, control.key());
    }
}

void PluginOptionsWidget::load()
{
    const std::unique_ptr<QSettings> settings(CrowSettings::open());
    for (const ProviderOptionSpec &spec : std::as_const(m_specs)) {
        QWidget *control = m_controls.value(spec.key, nullptr);
        if (control == nullptr)
            continue;

        const QVariant value = settings->value(settingsKeyFor(m_pluginId, spec), spec.defaultValue);
        if (auto *box = qobject_cast<QCheckBox *>(control))
            box->setChecked(value.toBool());
        else if (auto *spin = qobject_cast<QSpinBox *>(control))
            spin->setValue(value.toInt());
        else if (auto *edit = qobject_cast<QLineEdit *>(control))
            edit->setText(value.toString());
    }
}

void PluginOptionsWidget::save() const
{
    const std::unique_ptr<QSettings> settings(CrowSettings::open());
    for (const ProviderOptionSpec &spec : std::as_const(m_specs)) {
        QWidget *control = m_controls.value(spec.key, nullptr);
        if (control == nullptr)
            continue;

        const QString key = settingsKeyFor(m_pluginId, spec);
        if (auto *box = qobject_cast<QCheckBox *>(control))
            settings->setValue(key, box->isChecked());
        else if (auto *spin = qobject_cast<QSpinBox *>(control))
            settings->setValue(key, spin->value());
        else if (auto *edit = qobject_cast<QLineEdit *>(control))
            settings->setValue(key, spec.type == QMetaType::QByteArray ? QVariant(edit->text().toLocal8Bit()) : QVariant(edit->text()));
    }
}

void PluginOptionsWidget::restoreDefaults()
{
    for (const ProviderOptionSpec &spec : std::as_const(m_specs)) {
        QWidget *control = m_controls.value(spec.key, nullptr);
        if (control == nullptr)
            continue;

        if (auto *box = qobject_cast<QCheckBox *>(control))
            box->setChecked(spec.defaultValue.toBool());
        else if (auto *spin = qobject_cast<QSpinBox *>(control))
            spin->setValue(spec.defaultValue.toInt());
        else if (auto *edit = qobject_cast<QLineEdit *>(control)) {
            // Applied at once, like the other controls: there is no typing to
            // wait for.
            const QString value = spec.defaultValue.toString();
            if (edit->text() != value) {
                edit->setText(value);
                Q_EMIT optionChanged(spec.key, value);
            }
        }
    }
}
