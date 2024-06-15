/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef ADDLANGDIALOG_H
#define ADDLANGDIALOG_H

#include "qonlinetranslator.h"

#include <QDialog>

class QListWidget;
class QAbstractButton;
class QShortcut;

namespace Ui
{
class LanguagesDialog;
}

class LanguagesDialog : public QDialog
{
    Q_OBJECT
    Q_DISABLE_COPY(LanguagesDialog)

public:
    explicit LanguagesDialog(const QVector<QOnlineTranslator::Language> &currentLang, QWidget *parent = nullptr);
    ~LanguagesDialog() override;

    QVector<QOnlineTranslator::Language> languages() const;

public slots:
    void accept() override;

private slots:
    void filterLanguages(const QString &text);

    void moveLanguageRight();
    void moveLanguageLeft();
    void moveLanguageUp();
    void moveLanguageDown();

    void checkVerticalMovement(int row);

private:
    static void addLanguage(QListWidget *widget, QOnlineTranslator::Language lang);
    static void moveLanguageVertically(QListWidget *widget, int offset);
    static void moveLanguageHorizontally(QListWidget *from, QListWidget *to, QAbstractButton *addButton, QAbstractButton *removeButton);

    Ui::LanguagesDialog *ui;
    QShortcut *m_searchShortcut;
    QShortcut *m_acceptShortcut;
    QVector<QOnlineTranslator::Language> m_languages;
};

#endif // ADDLANGDIALOG_H
