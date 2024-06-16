/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LANGUAGEBUTTONSWIDGET_H
#define LANGUAGEBUTTONSWIDGET_H

#include "settings/appsettings.h"

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

    const QVector<OnlineTranslator::Language> &languages() const;
    void setLanguages(const QVector<OnlineTranslator::Language> &languages);

    OnlineTranslator::Language checkedLanguage() const;
    OnlineTranslator::Language previousCheckedLanguage() const;
    OnlineTranslator::Language language(int id) const;
    bool checkLanguage(OnlineTranslator::Language lang);
    void setLanguageFormat(AppSettings::LanguageFormat languageFormat);

    int checkedId() const;
    bool isAutoButtonChecked() const;
    void retranslate();

    static QIcon countryIcon(OnlineTranslator::Language lang);
    static void swapCurrentLanguages(LanguageButtonsWidget *first, LanguageButtonsWidget *second);

    static constexpr int autoButtonId()
    {
        return s_autoButtonId;
    }

signals:
    void buttonChecked(int id);
    void languageAdded(OnlineTranslator::Language lang);
    void languagesChanged(const QVector<OnlineTranslator::Language> &languages);
    void autoLanguageChanged(OnlineTranslator::Language lang);

public slots:
    void checkAutoButton();
    void checkButton(int id);
    void addLanguage(OnlineTranslator::Language lang);
    void setAutoLanguage(OnlineTranslator::Language lang);

private slots:
    void editLanguages();
    void savePreviousToggledButton(int id, bool checked);
    void checkAvailableScreenWidth();
    void minimizeWindowWidth();

private:
    void changeEvent(QEvent *event) override;

    void setWindowWidthCheckEnabled(bool enable) const;
    void addOrCheckLanguage(OnlineTranslator::Language lang);
    void addButton(OnlineTranslator::Language lang);
    void setButtonLanguage(QAbstractButton *button, OnlineTranslator::Language lang);

    QString languageString(OnlineTranslator::Language lang);

    static constexpr int s_autoButtonId = -2; // -1 is reserved by QButtonGroup

    Ui::LanguageButtonsWidget *ui;
    QButtonGroup *m_buttonGroup;

    QVector<OnlineTranslator::Language> m_languages;
    OnlineTranslator::Language m_autoLang = OnlineTranslator::Auto;
    AppSettings::LanguageFormat m_languageFormat = AppSettings::FullName;
    int m_previousCheckedId = 0;
};

#endif // LANGUAGEBUTTONSWIDGET_H
