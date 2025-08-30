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
#include <QDebug>
#include <QMessageBox>
#include <QScreen>
#include <QTimer>
#include <QToolButton>

using namespace std::chrono_literals;

LanguageButtonsWidget::LanguageButtonsWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::LanguageButtonsWidget)
    , m_buttonGroup(new QButtonGroup(this))
{
    ui->setupUi(this);
    addButton(Language::autoLanguage());
    m_buttonGroup->button(s_autoButtonId)->setChecked(true);
    setWindowWidthCheckEnabled(true);
    connect(m_buttonGroup, &QButtonGroup::idToggled, this, &LanguageButtonsWidget::savePreviousToggledButton);
}

LanguageButtonsWidget::~LanguageButtonsWidget()
{
    delete ui;
}

const QVector<Language> &LanguageButtonsWidget::languages() const
{
    return m_languages;
}

void LanguageButtonsWidget::setLanguages(const QVector<Language> &languages)
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

    updateButtonVisibility();

    emit languagesChanged(m_languages);
}

Language LanguageButtonsWidget::checkedLanguage() const
{
    return language(m_buttonGroup->checkedId());
}

Language LanguageButtonsWidget::previousCheckedLanguage() const
{
    return language(m_previousCheckedId);
}

Language LanguageButtonsWidget::language(int id) const
{
    if (id == s_autoButtonId)
        return m_autoLanguage;

    return m_languages[id];
}

bool LanguageButtonsWidget::checkLanguage(const Language &language)
{
    // Select auto button
    if (language == Language::autoLanguage()) {
        checkAutoButton();
        return true;
    }

    // Exit the function if the current language already has a button
    for (int i = 0; i < m_languages.size(); ++i) {
        if (language == m_languages[i]) {
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

void LanguageButtonsWidget::setAutoButtonVisible(bool visible)
{
    m_autoButtonVisible = visible;
    QAbstractButton *autoButton = m_buttonGroup->button(s_autoButtonId);
    if (autoButton != nullptr) {
        autoButton->setVisible(visible);
    }
}

void LanguageButtonsWidget::retranslate()
{
    for (int i = 0; i < m_languages.size(); ++i)
        setButtonLanguage(m_buttonGroup->button(i), m_languages[i]);
    setButtonLanguage(m_buttonGroup->button(s_autoButtonId), m_autoLanguage);
}

void LanguageButtonsWidget::swapCurrentLanguages(LanguageButtonsWidget *first, LanguageButtonsWidget *second)
{
    // Backup first widget buttons properties
    const Language sourceLanguage = first->checkedLanguage();
    const Language destLanguage = second->checkedLanguage();

    if (second->isAutoButtonChecked())
        first->checkAutoButton();
    else
        first->addOrCheckLanguage(destLanguage);

    // Insert current source language to the second widget
    second->addOrCheckLanguage(sourceLanguage);
}

void LanguageButtonsWidget::checkAutoButton()
{
    checkButton(s_autoButtonId);
}

void LanguageButtonsWidget::checkButton(int id)
{
    QAbstractButton *button = m_buttonGroup->button(id);
    if (button != nullptr)
        button->setChecked(true);
    else
        m_buttonGroup->button(s_autoButtonId)->setChecked(true);
}

void LanguageButtonsWidget::addLanguage(const Language &language)
{
    Q_ASSERT_X(!m_languages.contains(language), "addLanguage", "Language already exists");

    m_languages.append(language);
    addButton(language);
    emit languageAdded(language);
}

void LanguageButtonsWidget::setAutoLanguage(const Language &language)
{
    if (m_autoLanguage == language)
        return;

    m_autoLanguage = language;
    setButtonLanguage(m_buttonGroup->button(s_autoButtonId), m_autoLanguage);
    emit autoLanguageChanged(m_autoLanguage);
}

void LanguageButtonsWidget::editLanguages()
{
    LanguagesDialog langDialog(m_languages);
    if (langDialog.exec() == QDialog::Accepted) {
        setLanguages(langDialog.languages());
    }
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

void LanguageButtonsWidget::addOrCheckLanguage(const Language &language)
{
    if (checkLanguage(language))
        return;

    addLanguage(language);
    m_buttonGroup->buttons().constLast()->setChecked(true);
}

void LanguageButtonsWidget::addButton(const Language &language)
{
    auto *button = new QToolButton;
    button->setCheckable(true);
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    button->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred); // To make the same size for all buttons (without it "Auto" button can look different)

    // Use special id for "Auto" button to count all other languages from 0
    m_buttonGroup->addButton(button, language == Language::autoLanguage() ? s_autoButtonId : m_buttonGroup->buttons().size() - 1);

    setButtonLanguage(button, language);

    // Insert all languages after "Edit" button
    ui->languagesLayout->insertWidget(ui->languagesLayout->count() - 1, button);
}

void LanguageButtonsWidget::setButtonLanguage(QAbstractButton *button, const Language &language)
{
    const QString languageName = languageString(language);
    if (button == m_buttonGroup->button(s_autoButtonId)) {
        if (language == Language::autoLanguage())
            button->setText(tr("Auto"));
        else
            button->setText(tr("Auto") + " (" + languageName + ")");
    } else {
        button->setText(languageName);
    }

    QString tooltip;
    if (language.hasQLocaleEquivalent()) {
        const QLocale locale = language.toQLocale();
        tooltip = locale.nativeLanguageName().isEmpty() ? QLocale::languageToString(locale.language()) : locale.nativeLanguageName();
    } else {
        tooltip = language.nativeName().isEmpty() ? language.toString() : language.nativeName();
    }
    button->setToolTip(tooltip);
}

QString LanguageButtonsWidget::languageString(const Language &language)
{
    switch (m_languageFormat) {
    case AppSettings::FullName: {
        if (language.hasQLocaleEquivalent()) {
            const QLocale locale = language.toQLocale();
            QString baseName = QLocale::languageToString(locale.language());

            const QString bcp47 = locale.bcp47Name();
            if (bcp47 != baseName.toLower() && !bcp47.isEmpty()) {
                switch (locale.language()) {
                case QLocale::English:
                case QLocale::Spanish:
                case QLocale::Portuguese:
                case QLocale::French:
                case QLocale::German:
                case QLocale::Arabic:
                case QLocale::Chinese:
                    return QString("%1 (%2)").arg(baseName, bcp47);
                default:
                    const QLocale defaultLocale(locale.language());
                    if (locale.bcp47Name() != defaultLocale.bcp47Name()) {
                        return QString("%1 (%2)").arg(baseName, bcp47);
                    }
                    break;
                }
            }

            return baseName;
        } else {
            return language.toString();
        }
    }
    case AppSettings::IsoCode: {
        if (language.hasQLocaleEquivalent()) {
            const QLocale locale = language.toQLocale();
            QString fullCode = locale.bcp47Name();
            return fullCode;
        } else {
            return language.toCode();
        }
    }
    default:
        Q_UNREACHABLE();
    }
}

void LanguageButtonsWidget::setSupportedLanguages(const QVector<Language> &supportedLanguages)
{
    m_supportedLanguages = supportedLanguages;
    updateButtonVisibility();
}

void LanguageButtonsWidget::clearSupportedLanguages()
{
    m_supportedLanguages.clear();
    updateButtonVisibility();
}

void LanguageButtonsWidget::updateButtonVisibility()
{
    if (m_supportedLanguages.isEmpty()) {
        const QList<QAbstractButton *> buttons = m_buttonGroup->buttons();
        for (QAbstractButton *button : buttons) {
            if ((button != nullptr) && m_buttonGroup->id(button) != s_autoButtonId) {
                button->setVisible(true);
                button->setEnabled(true);
            }
        }
        return;
    }

    QAbstractButton *autoButton = m_buttonGroup->button(s_autoButtonId);
    if (autoButton != nullptr) {
        autoButton->setVisible(m_autoButtonVisible);
        autoButton->setEnabled(m_autoButtonVisible);
    }

    for (int i = 0; i < m_languages.size(); ++i) {
        QAbstractButton *button = m_buttonGroup->button(i);
        if (button != nullptr) {
            const Language &language = m_languages.at(i);
            const bool isSupported = isLanguageSupported(language);
            button->setVisible(isSupported);
            button->setEnabled(isSupported);
        }
    }
}

bool LanguageButtonsWidget::isLanguageSupported(const Language &language) const
{
    if (language == Language::autoLanguage()) {
        return true;
    }

    for (const Language &supportedLanguage : m_supportedLanguages) {
        if (supportedLanguage == language) {
            return true;
        }
        // Also check if they have equivalent QLocale languages
        if (supportedLanguage.hasQLocaleEquivalent() && language.hasQLocaleEquivalent()) {
            const QLocale supportedLocale = supportedLanguage.toQLocale();
            const QLocale currentLocale = language.toQLocale();
            if (supportedLocale.language() == currentLocale.language()) {
                return true;
            }
        }
    }

    return false;
}
