/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LANGUAGEBUTTONSWIDGET_H
#define LANGUAGEBUTTONSWIDGET_H

#include "language.h"
#include "settings/appsettings.h"

#include <QLocale>
#include <QWidget>

class QAbstractButton;
class QButtonGroup;

namespace Ui
{
class LanguageButtonsWidget;
}

class LanguageButtonsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit LanguageButtonsWidget(QWidget *parent = nullptr);
    ~LanguageButtonsWidget() override;

    const QVector<Language> &languages() const;
    void setLanguages(const QVector<Language> &languages);

    Language checkedLanguage() const;
    Language previousCheckedLanguage() const;
    Language language(int id) const;
    bool checkLanguage(const Language &language);
    void setLanguageFormat(AppSettings::LanguageFormat languageFormat);

    void setSupportedLanguages(const QVector<Language> &supportedLanguages);
    void clearSupportedLanguages();

    int checkedId() const;
    bool isAutoButtonChecked() const;
    void setAutoButtonVisible(bool visible);
    void retranslate();

    static void swapCurrentLanguages(LanguageButtonsWidget *first, LanguageButtonsWidget *second);

    static constexpr int autoButtonId()
    {
        return s_autoButtonId;
    }

signals:
    void buttonChecked(int id);
    void languageAdded(const Language &language);
    void languagesChanged(const QVector<Language> &languages);
    void autoLanguageChanged(const Language &language);

public slots:
    void checkAutoButton();
    void checkButton(int id);
    void addLanguage(const Language &language);
    void setAutoLanguage(const Language &language);

private slots:
    void editLanguages();
    void savePreviousToggledButton(int id, bool checked);
    void checkAvailableScreenWidth();
    void minimizeWindowWidth();

private:
    void changeEvent(QEvent *event) override;

    void setWindowWidthCheckEnabled(bool enable) const;
    void addOrCheckLanguage(const Language &language);
    void addButton(const Language &language);
    void setButtonLanguage(QAbstractButton *button, const Language &language);
    void updateButtonVisibility();
    bool isLanguageSupported(const Language &language) const;

    QString languageString(const Language &language);

    static constexpr int s_autoButtonId = -2; // -1 is reserved by QButtonGroup

    Ui::LanguageButtonsWidget *ui;
    QButtonGroup *m_buttonGroup;

    QVector<Language> m_languages;
    QVector<Language> m_supportedLanguages;
    Language m_autoLanguage;
    AppSettings::LanguageFormat m_languageFormat = AppSettings::FullName;
    int m_previousCheckedId = 0;
    bool m_autoButtonVisible = true;
};

#endif // LANGUAGEBUTTONSWIDGET_H
