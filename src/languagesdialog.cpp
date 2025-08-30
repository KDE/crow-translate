/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "languagesdialog.h"
#include "ui_languagesdialog.h"

#include "languagebuttonswidget.h"

#include <QPushButton>
#include <QSet>
#include <QShortcut>

LanguagesDialog::LanguagesDialog(const QVector<Language> &currentLanguages, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::LanguagesDialog)
    , m_searchShortcut(new QShortcut(QStringLiteral("Ctrl+F"), this))
    , m_acceptShortcut(new QShortcut(QStringLiteral("Ctrl+Enter"), this))
{
    ui->setupUi(this);
    ui->searchEdit->setPlaceholderText(tr("Filter (%1)").arg(m_searchShortcut->key().toString()));
    connect(m_searchShortcut, &QShortcut::activated, ui->searchEdit, qOverload<>(&QLineEdit::setFocus));
    connect(m_acceptShortcut, &QShortcut::activated, this, &LanguagesDialog::accept);

    // Load languages from Language::allLanguages()
    const auto availableLanguages = Language::allLanguages();
    QSet<Language> uniqueLanguages; // Track unique languages to avoid duplicates

    for (const Language &language : availableLanguages) {
        if (language.isValid() && !currentLanguages.contains(language)
            && !uniqueLanguages.contains(language)) {
            uniqueLanguages.insert(language);
            addLanguage(ui->availableLanguagesListWidget, language);
        }
    }
    ui->availableLanguagesListWidget->setCurrentRow(0);

    for (const Language &language : currentLanguages)
        addLanguage(ui->currentLanguagesListWidget, language);

    if (ui->currentLanguagesListWidget->count() != 0) {
        ui->currentLanguagesListWidget->setCurrentRow(0);
        ui->moveLeftButton->setEnabled(true);
    }
}

LanguagesDialog::~LanguagesDialog()
{
    delete ui;
}

const QVector<Language> &LanguagesDialog::languages() const
{
    return m_languages;
}

void LanguagesDialog::accept()
{
    QDialog::accept();

    m_languages.reserve(ui->currentLanguagesListWidget->count());
    for (int i = 0; i < ui->currentLanguagesListWidget->count(); ++i) {
        QListWidgetItem *item = ui->currentLanguagesListWidget->item(i);
        m_languages.append(item->data(Qt::UserRole).value<Language>());
    }
}

void LanguagesDialog::filterLanguages(const QString &text)
{
    bool isItemSelected = false;
    for (int i = 0; i < ui->availableLanguagesListWidget->count(); ++i) {
        QListWidgetItem *item = ui->availableLanguagesListWidget->item(i);
        if (item->text().contains(text, Qt::CaseInsensitive)) {
            item->setHidden(false);
            if (!isItemSelected) {
                ui->availableLanguagesListWidget->setCurrentItem(item); // Select first unhidden item
                isItemSelected = true;
            }
        } else {
            item->setHidden(true);
        }
    }

    // Disable Ok button if no item selected
    ui->dialogButtonBox->button(QDialogButtonBox::Ok)->setEnabled(isItemSelected);
}

void LanguagesDialog::moveLanguageRight()
{
    moveLanguageHorizontally(ui->availableLanguagesListWidget, ui->currentLanguagesListWidget, ui->moveRightButton, ui->moveLeftButton);
}

void LanguagesDialog::moveLanguageLeft()
{
    // Block signals to emit index change after item deletion
    ui->currentLanguagesListWidget->blockSignals(true);
    moveLanguageHorizontally(ui->currentLanguagesListWidget, ui->availableLanguagesListWidget, ui->moveLeftButton, ui->moveRightButton);
    ui->currentLanguagesListWidget->blockSignals(false);

    emit ui->currentLanguagesListWidget->currentRowChanged(ui->currentLanguagesListWidget->currentRow());
}

void LanguagesDialog::moveLanguageUp()
{
    moveLanguageVertically(ui->currentLanguagesListWidget, -1);
}

void LanguagesDialog::moveLanguageDown()
{
    moveLanguageVertically(ui->currentLanguagesListWidget, +1);
}

void LanguagesDialog::checkVerticalMovement(int row)
{
    if (row == -1) {
        ui->moveUpButton->setEnabled(false);
        ui->moveDownButton->setEnabled(false);
        return;
    }

    // Disable "Up" button for first element and "Down" for last
    ui->moveUpButton->setEnabled(row != 0);
    ui->moveDownButton->setEnabled(row != ui->currentLanguagesListWidget->count() - 1);
}

void LanguagesDialog::addLanguage(QListWidget *widget, const Language &language)
{
    auto *item = new QListWidgetItem;
    item->setText(language.displayName());
    item->setData(Qt::UserRole, QVariant::fromValue(language));
    widget->addItem(item);
}

void LanguagesDialog::moveLanguageVertically(QListWidget *widget, int offset)
{
    const int currentRow = widget->currentRow();
    widget->insertItem(currentRow + offset, widget->takeItem(currentRow));
    widget->setCurrentRow(currentRow + offset);
}

void LanguagesDialog::moveLanguageHorizontally(QListWidget *from, QListWidget *to, QAbstractButton *addButton, QAbstractButton *removeButton)
{
    QListWidgetItem *item = from->takeItem(from->currentRow());
    to->addItem(item);
    to->setCurrentItem(item);

    removeButton->setEnabled(true);
    if (from->count() == 0)
        addButton->setEnabled(false);
}
