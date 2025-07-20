/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "popupwindow.h"
#include "ui_popupwindow.h"

#include "languagebuttonswidget.h"
#include "mainwindow.h"
#include "speakbuttons.h"
#include "translationedit.h"

#include <QCloseEvent>
#include <QScreen>
#include <QShortcut>
#include <QTimer>

PopupWindow::PopupWindow(MainWindow *parent)
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint)
    , ui(new Ui::PopupWindow)
    , m_closeWindowsShortcut(new QShortcut(this))
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    // Engine
    ui->engineComboBox->setCurrentIndex(parent->engineCombobox()->currentIndex());
    connect(ui->engineComboBox, qOverload<int>(&QComboBox::currentIndexChanged), parent->engineCombobox(), &QComboBox::setCurrentIndex);

    // Translation edit
    ui->translationEdit->setFont(parent->translationEdit()->font());
    connect(parent->translationEdit(), &TranslationEdit::translationDataParsed, ui->translationEdit, &QTextEdit::setHtml);

    // Player buttons
    ui->sourceSpeakButtons->setMediaPlayer(parent->sourceSpeakButtons()->mediaPlayer());
    ui->translationSpeakButtons->setMediaPlayer(parent->translationSpeakButtons()->mediaPlayer());
    ui->sourceSpeakButtons->setSpeakShortcut(parent->sourceSpeakButtons()->speakShortcut());
    ui->translationSpeakButtons->setSpeakShortcut(parent->translationSpeakButtons()->speakShortcut());
    connect(ui->sourceSpeakButtons, &SpeakButtons::playerMediaRequested, parent->sourceSpeakButtons(), &SpeakButtons::playerMediaRequested);
    connect(ui->translationSpeakButtons, &SpeakButtons::playerMediaRequested, parent->translationSpeakButtons(), &SpeakButtons::playerMediaRequested);

    // Language buttons
    connectLanguageButtons(ui->sourceLanguagesWidget, parent->sourceLanguageButtons());
    connectLanguageButtons(ui->translationLanguagesWidget, parent->translationLanguageButtons());

    // Buttons
    ui->copyTranslationButton->setShortcut(parent->copyTranslationButton()->shortcut());
    connect(ui->copyTranslationButton, &QToolButton::clicked, parent->copyTranslationButton(), &QToolButton::click);
    connect(ui->swapButton, &QToolButton::clicked, parent->swapButton(), &QToolButton::click);
    connect(ui->copySourceButton, &QToolButton::clicked, parent->copySourceButton(), &QToolButton::click);
    connect(ui->copyAllTranslationButton, &QToolButton::clicked, parent->copyAllTranslationButton(), &QToolButton::click);

    // Close window shortcut
    m_closeWindowsShortcut->setKey(parent->closeWindowShortcut());
    connect(m_closeWindowsShortcut, &QShortcut::activated, this, &PopupWindow::close);

    loadSettings();
}

PopupWindow::~PopupWindow()
{
    ui->sourceSpeakButtons->pauseSpeaking();
    ui->translationSpeakButtons->pauseSpeaking();
    delete ui;
}

void PopupWindow::loadSettings()
{
    const AppSettings settings;
    setWindowOpacity(settings.popupOpacity());
    resize(settings.popupWidth(), settings.popupHeight());

    ui->sourceLanguagesWidget->setLanguageFormat(settings.popupLanguageFormat());
    ui->translationLanguagesWidget->setLanguageFormat(settings.popupLanguageFormat());

    if (settings.popupWindowTimeout() > 0) {
        m_closeWindowTimer = new QTimer(this);
        m_closeWindowTimer->callOnTimeout(this, &PopupWindow::close);
        m_closeWindowTimer->setInterval(settings.popupWindowTimeout() * 1000);
        m_closeWindowTimer->start();
    }
}

// Move popup to cursor and prevent appearing outside the screen
void PopupWindow::showEvent(QShowEvent *event)
{
    QPoint position = QCursor::pos(); // Cursor position
    const QSize availableSize = QGuiApplication::screenAt(position)->availableSize();

    if (availableSize.width() - position.x() - geometry().width() < 0) {
        position.rx() -= frameGeometry().width();
        if (position.x() < 0)
            position.rx() = 0;
    }
    if (availableSize.height() - position.y() - geometry().height() < 0) {
        position.ry() -= frameGeometry().height();
        if (position.y() < 0)
            position.ry() = 0;
    }

    move(position);
    QWidget::showEvent(event);
}

bool PopupWindow::event(QEvent *event)
{
    switch (event->type()) {
    case QEvent::WindowDeactivate:
        // Do not close the window if the language selection menu is active
        if (QApplication::activeModalWidget() == nullptr)
            close();
        break;
    case QEvent::Leave:
        // Start timer, if mouse left window
        if (m_closeWindowTimer)
            m_closeWindowTimer->start();
        break;
    case QEvent::Enter:
        // Stop timer, if mouse enter window
        if (m_closeWindowTimer)
            m_closeWindowTimer->stop();
        break;
    default:
        break;
    }

    return QWidget::event(event);
}

void PopupWindow::connectLanguageButtons(LanguageButtonsWidget *popupButtons, const LanguageButtonsWidget *mainWindowButtons)
{
    popupButtons->setLanguages(mainWindowButtons->languages());
    popupButtons->checkButton(mainWindowButtons->checkedId());
    connect(popupButtons, &LanguageButtonsWidget::buttonChecked, mainWindowButtons, &LanguageButtonsWidget::checkButton);
    connect(popupButtons, &LanguageButtonsWidget::languagesChanged, mainWindowButtons, &LanguageButtonsWidget::setLanguages);
    connect(mainWindowButtons, &LanguageButtonsWidget::buttonChecked, popupButtons, &LanguageButtonsWidget::checkButton);
    connect(mainWindowButtons, &LanguageButtonsWidget::autoLanguageChanged, popupButtons, &LanguageButtonsWidget::setAutoLanguage);
    connect(mainWindowButtons, &LanguageButtonsWidget::languageAdded, popupButtons, &LanguageButtonsWidget::addLanguage);
}
