/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "languagebuttonswidget.h"
#include "ui_languagebuttonswidget.h"

#include "languagesdialog.h"
#include "screenwatcher.h"

#include <QButtonGroup>
#include <QMessageBox>
#include <QScreen>
#include <QTimer>
#include <QToolButton>

using namespace std::chrono_literals;

LanguageButtonsWidget::LanguageButtonsWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::LanguageButtonsWidget)
    , m_buttonGroup(new QButtonGroup)
{
    ui->setupUi(this);
    addButton(OnlineTranslator::Auto);
    m_buttonGroup->button(s_autoButtonId)->setChecked(true);
    setWindowWidthCheckEnabled(true);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    connect(m_buttonGroup, &QButtonGroup::idToggled, this, &LanguageButtonsWidget::savePreviousToggledButton);
#else
    connect(m_buttonGroup, qOverload<int, bool>(&QButtonGroup::buttonToggled), this, &LanguageButtonsWidget::savePreviousToggledButton);
#endif
}

LanguageButtonsWidget::~LanguageButtonsWidget()
{
    delete ui;
}

const QVector<OnlineTranslator::Language> &LanguageButtonsWidget::languages() const
{
    return m_languages;
}

void LanguageButtonsWidget::setLanguages(const QVector<OnlineTranslator::Language> &languages)
{
    if (m_languages == languages)
        return;

    // Add or set new languages
    for (int i = 0; i < languages.size(); ++i) {
        // Use -1 to ignore "Auto" button
        if (i < m_buttonGroup->buttons().size() - 1) {
            if (m_languages[i] != languages[i])
                setButtonLanguage(m_buttonGroup->button(i), languages[i]);
        } else {
            addButton(languages[i]);
        }
    }

    m_languages = languages;

    // Delete extra buttons
    for (int i = languages.size(); languages.size() != m_buttonGroup->buttons().size() - 1; ++i) {
        QAbstractButton *button = m_buttonGroup->button(i);
        m_buttonGroup->removeButton(button);
        delete button;
    }

    emit languagesChanged(m_languages);
}

OnlineTranslator::Language LanguageButtonsWidget::checkedLanguage() const
{
    return language(m_buttonGroup->checkedId());
}

OnlineTranslator::Language LanguageButtonsWidget::previousCheckedLanguage() const
{
    return language(m_previousCheckedId);
}

OnlineTranslator::Language LanguageButtonsWidget::language(int id) const
{
    if (id == s_autoButtonId)
        return m_autoLang;

    return m_languages[id];
}

bool LanguageButtonsWidget::checkLanguage(OnlineTranslator::Language lang)
{
    // Select auto button
    if (lang == OnlineTranslator::Auto) {
        checkAutoButton();
        return true;
    }

    // Exit the function if the current language already has a button
    for (int i = 0; i < m_languages.size(); ++i) {
        if (lang == m_languages[i]) {
            checkButton(i);
            return true;
        }
    }

    return false;
}

void LanguageButtonsWidget::setLanguageFormat(AppSettings::LanguageFormat languageFormat)
{
    if (m_languageFormat == languageFormat)
        return;

    m_languageFormat = languageFormat;
    retranslate();
}

int LanguageButtonsWidget::checkedId() const
{
    return m_buttonGroup->checkedId();
}

bool LanguageButtonsWidget::isAutoButtonChecked() const
{
    return m_buttonGroup->checkedId() == s_autoButtonId;
}

void LanguageButtonsWidget::retranslate()
{
    for (int i = 0; i < m_languages.size(); ++i)
        setButtonLanguage(m_buttonGroup->button(i), m_languages[i]);
    setButtonLanguage(m_buttonGroup->button(s_autoButtonId), m_autoLang);
}

void LanguageButtonsWidget::swapCurrentLanguages(LanguageButtonsWidget *first, LanguageButtonsWidget *second)
{
    // Backup first widget buttons properties
    const OnlineTranslator::Language sourceLang = first->checkedLanguage();
    const bool isSourceAutoButtonChecked = first->isAutoButtonChecked();

    // Insert current translation language to the first widget
    if (second->isAutoButtonChecked())
        first->checkAutoButton();
    else
        first->addOrCheckLanguage(second->checkedLanguage());

    // Insert current source language to the second widget
    if (isSourceAutoButtonChecked)
        second->checkAutoButton();
    else
        second->addOrCheckLanguage(sourceLang);
}

void LanguageButtonsWidget::checkAutoButton()
{
    checkButton(s_autoButtonId);
}

void LanguageButtonsWidget::checkButton(int id)
{
    QAbstractButton *button = m_buttonGroup->button(id);
    if (button)
        button->setChecked(true);
    else
        m_buttonGroup->button(s_autoButtonId)->setChecked(true);
}

void LanguageButtonsWidget::addLanguage(OnlineTranslator::Language lang)
{
    Q_ASSERT_X(!m_languages.contains(lang), "addLanguage", "Language already exists");

    m_languages.append(lang);
    addButton(lang);
    emit languageAdded(lang);
}

void LanguageButtonsWidget::setAutoLanguage(OnlineTranslator::Language lang)
{
    if (m_autoLang == lang)
        return;

    m_autoLang = lang;
    setButtonLanguage(m_buttonGroup->button(s_autoButtonId), m_autoLang);
    emit autoLanguageChanged(m_autoLang);
}

void LanguageButtonsWidget::editLanguages()
{
    LanguagesDialog langDialog(languages());
    if (langDialog.exec() == QDialog::Accepted)
        setLanguages(langDialog.languages());
}

void LanguageButtonsWidget::savePreviousToggledButton(int id, bool checked)
{
    if (!checked)
        m_previousCheckedId = id;
    else
        emit buttonChecked(id);
}

void LanguageButtonsWidget::checkAvailableScreenWidth()
{
    // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
    if (ScreenWatcher::isWidthFitScreen(window()))
        return;

    // Try resize first
    window()->resize(window()->minimumWidth(), window()->height());

    if (ScreenWatcher::isWidthFitScreen(window()))
        return;

    QMessageBox message;
    message.setIcon(QMessageBox::Information);
    message.setText(tr("Window width is larger than screen due to the languages on the panel."));
    message.setInformativeText(tr("Please reduce added languages."));
    if (message.exec() == QMessageBox::Ok) {
        const int languagesCountBefore = m_languages.size();
        // Temporary disable connection to this slot to trigger it manually after resize
        setWindowWidthCheckEnabled(false);
        editLanguages();
        setWindowWidthCheckEnabled(true);

        if (m_languages.size() < languagesCountBefore)
            minimizeWindowWidth(); // For unknown reason QWindow::minimumWidthChanged is not emited in this case, so wait for changes manually
        else
            checkAvailableScreenWidth();
    }
}

void LanguageButtonsWidget::minimizeWindowWidth()
{
    if (window()->width() == window()->minimumWidth()) {
        QTimer::singleShot(100ms, this, &LanguageButtonsWidget::minimizeWindowWidth);
        return;
    }

    window()->resize(window()->minimumWidth(), window()->height());
    checkAvailableScreenWidth();
}

void LanguageButtonsWidget::changeEvent(QEvent *event)
{
    switch (event->type()) {
    case QEvent::LanguageChange:
        if (m_languageFormat != AppSettings::IsoCode)
            retranslate();
        break;
    default:
        QWidget::changeEvent(event);
    }
}

void LanguageButtonsWidget::setWindowWidthCheckEnabled(bool enable) const
{
    if (enable)
        connect(this, &LanguageButtonsWidget::languagesChanged, this, &LanguageButtonsWidget::checkAvailableScreenWidth, Qt::QueuedConnection);
    else
        disconnect(this, &LanguageButtonsWidget::languagesChanged, this, &LanguageButtonsWidget::checkAvailableScreenWidth);
}

void LanguageButtonsWidget::addOrCheckLanguage(OnlineTranslator::Language lang)
{
    if (checkLanguage(lang))
        return;

    addLanguage(lang);
    m_buttonGroup->buttons().constLast()->setChecked(true);
}

void LanguageButtonsWidget::addButton(OnlineTranslator::Language lang)
{
    auto *button = new QToolButton;
    button->setCheckable(true);
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    button->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred); // To make the same size for all buttons (without it "Auto" button can look different)

    // Use special id for "Auto" button to count all other languages from 0
    m_buttonGroup->addButton(button, lang == OnlineTranslator::Auto ? s_autoButtonId : m_buttonGroup->buttons().size() - 1);

    setButtonLanguage(button, lang);

    // Insert all languages after "Edit" button
    ui->languagesLayout->insertWidget(ui->languagesLayout->count() - 1, button);
}

void LanguageButtonsWidget::setButtonLanguage(QAbstractButton *button, OnlineTranslator::Language lang)
{
    const QString langName = languageString(lang);
    if (button == m_buttonGroup->button(s_autoButtonId)) {
        if (lang == OnlineTranslator::Auto)
            button->setText(tr("Auto"));
        else
            button->setText(tr("Auto") + " (" + langName + ")");
    } else {
        button->setText(langName);
    }

    button->setToolTip(OnlineTranslator::languageName(lang));
}

QString LanguageButtonsWidget::languageString(OnlineTranslator::Language lang)
{
    switch (m_languageFormat) {
    case AppSettings::FullName:
        return OnlineTranslator::languageName(lang);
    case AppSettings::IsoCode:
        return OnlineTranslator::languageCode(lang);
    default:
        Q_UNREACHABLE();
    }
}
