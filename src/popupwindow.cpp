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
#include "translationedit.h"

#include <QCloseEvent>
#include <QScreen>
#include <QShortcut>
#include <QTimer>

PopupWindow::PopupWindow(MainWindow *parent)
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint)
    , ui(new Ui::PopupWindow)
    , m_closeWindowsShortcut(new QShortcut(this))
    , m_parent(parent)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    // Engine
    if (parent->getEngineComboBox() != nullptr) {
        ui->engineComboBox->setCurrentIndex(parent->getEngineComboBox()->currentIndex());
        connect(ui->engineComboBox, qOverload<int>(&QComboBox::currentIndexChanged), parent->getEngineComboBox(), &QComboBox::setCurrentIndex);
    }

    // Translation edit
    if (parent->translationEdit() != nullptr) {
        ui->translationEdit->setFont(parent->translationEdit()->font());
        ui->translationEdit->setHtml(parent->translationEdit()->toHtml());
        m_textChangedConnection = connect(parent->translationEdit(), &QTextEdit::textChanged, [this]() {
            if (!m_parent.isNull() && ui && ui->translationEdit) {
                auto *translationEdit = m_parent->translationEdit();
                if (translationEdit != nullptr) {
                    ui->translationEdit->setHtml(translationEdit->toHtml());
                }
            }
        });
    }

    // TTS buttons - copy icons and connect to MainWindow TTS methods
    if (parent->sourcePlayPauseButton() != nullptr) {
        ui->sourcePlayPauseButton->setIcon(parent->sourcePlayPauseButton()->icon());
    }
    if (parent->sourceStopButton() != nullptr) {
        ui->sourceStopButton->setIcon(parent->sourceStopButton()->icon());
    }
    if (parent->translationPlayPauseButton() != nullptr) {
        ui->translationPlayPauseButton->setIcon(parent->translationPlayPauseButton()->icon());
    }
    if (parent->translationStopButton() != nullptr) {
        ui->translationStopButton->setIcon(parent->translationStopButton()->icon());
    }
    connect(ui->sourcePlayPauseButton, &QToolButton::clicked, parent, &MainWindow::sourcePlayPauseClicked);
    connect(ui->sourceStopButton, &QToolButton::clicked, parent, &MainWindow::sourceStopClicked);
    connect(ui->translationPlayPauseButton, &QToolButton::clicked, parent, &MainWindow::translationPlayPauseClicked);
    connect(ui->translationStopButton, &QToolButton::clicked, parent, &MainWindow::translationStopClicked);

    // Sync voice combo boxes - copy items and current selection
    if (parent->sourceVoiceComboBox() != nullptr) {
        for (int i = 0; i < parent->sourceVoiceComboBox()->count(); ++i) {
            ui->sourceVoiceComboBox->addItem(parent->sourceVoiceComboBox()->itemText(i), parent->sourceVoiceComboBox()->itemData(i));
        }
        ui->sourceVoiceComboBox->setCurrentIndex(parent->sourceVoiceComboBox()->currentIndex());
        connect(ui->sourceVoiceComboBox, qOverload<int>(&QComboBox::currentIndexChanged), parent->sourceVoiceComboBox(), &QComboBox::setCurrentIndex);
    }
    if (parent->translationVoiceComboBox() != nullptr) {
        for (int i = 0; i < parent->translationVoiceComboBox()->count(); ++i) {
            ui->translationVoiceComboBox->addItem(parent->translationVoiceComboBox()->itemText(i), parent->translationVoiceComboBox()->itemData(i));
        }
        ui->translationVoiceComboBox->setCurrentIndex(parent->translationVoiceComboBox()->currentIndex());
        connect(ui->translationVoiceComboBox, qOverload<int>(&QComboBox::currentIndexChanged), parent->translationVoiceComboBox(), &QComboBox::setCurrentIndex);
    }

    // Sync speaker combo boxes - copy items and current selection
    if (parent->sourceSpeakerComboBox() != nullptr) {
        for (int i = 0; i < parent->sourceSpeakerComboBox()->count(); ++i) {
            ui->sourceSpeakerComboBox->addItem(parent->sourceSpeakerComboBox()->itemText(i), parent->sourceSpeakerComboBox()->itemData(i));
        }
        ui->sourceSpeakerComboBox->setCurrentIndex(parent->sourceSpeakerComboBox()->currentIndex());
        connect(ui->sourceSpeakerComboBox, qOverload<int>(&QComboBox::currentIndexChanged), parent->sourceSpeakerComboBox(), &QComboBox::setCurrentIndex);
    }
    if (parent->translationSpeakerComboBox() != nullptr) {
        for (int i = 0; i < parent->translationSpeakerComboBox()->count(); ++i) {
            ui->translationSpeakerComboBox->addItem(parent->translationSpeakerComboBox()->itemText(i), parent->translationSpeakerComboBox()->itemData(i));
        }
        ui->translationSpeakerComboBox->setCurrentIndex(parent->translationSpeakerComboBox()->currentIndex());
        connect(ui->translationSpeakerComboBox, qOverload<int>(&QComboBox::currentIndexChanged), parent->translationSpeakerComboBox(), &QComboBox::setCurrentIndex);
    }

    // Language buttons
    if (parent->sourceLanguageButtons() != nullptr) {
        connectLanguageButtons(ui->sourceLanguagesWidget, parent->sourceLanguageButtons());
    }
    if (parent->translationLanguageButtons() != nullptr) {
        connectLanguageButtons(ui->translationLanguagesWidget, parent->translationLanguageButtons());
    }

    // Buttons
    if (parent->copyTranslationButton() != nullptr) {
        ui->copyTranslationButton->setShortcut(parent->copyTranslationButton()->shortcut());
        connect(ui->copyTranslationButton, &QToolButton::clicked, parent->copyTranslationButton(), &QToolButton::click);
    }
    if (parent->swapButton() != nullptr) {
        connect(ui->swapButton, &QToolButton::clicked, parent->swapButton(), &QToolButton::click);
    }
    if (parent->copySourceButton() != nullptr) {
        connect(ui->copySourceButton, &QToolButton::clicked, parent->copySourceButton(), &QToolButton::click);
    }
    if (parent->copyAllTranslationButton() != nullptr) {
        connect(ui->copyAllTranslationButton, &QToolButton::clicked, parent->copyAllTranslationButton(), &QToolButton::click);
    }

    // Close window shortcut
    m_closeWindowsShortcut->setKey(parent->closeWindowShortcut());
    connect(m_closeWindowsShortcut, &QShortcut::activated, this, &PopupWindow::close);

    loadSettings();
}

PopupWindow::~PopupWindow()
{
    if (m_textChangedConnection != nullptr) {
        disconnect(m_textChangedConnection);
    }
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
        if (m_closeWindowTimer != nullptr)
            m_closeWindowTimer->start();
        break;
    case QEvent::Enter:
        // Stop timer, if mouse enter window
        if (m_closeWindowTimer != nullptr)
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
}
