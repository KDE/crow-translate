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

#include <QAbstractItemModel>
#include <QCloseEvent>
#include <QComboBox>
#include <QDebug>
#include <QScreen>
#include <QShortcut>
#include <QTimer>
#include <QToolButton>

#include <utility>

PopupWindow::PopupWindow(MainWindow *parent)
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint)
    , ui(new Ui::PopupWindow)
    , m_closeWindowsShortcut(new QShortcut(this))
    , m_parent(parent)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    // Everything the pop-up shows of the main window's controls is a MIRROR,
    // not a snapshot, and the difference has produced three separate bugs.
    // The state it copies keeps moving after it is built: translatorStateChanged()
    // creates this window and only THEN calls refreshVoicesForSpokenLanguages(),
    // because the destination "auto" resolves to is not known until the
    // translation comes back - so a copy taken in this constructor shows the
    // voices of the language translated into LAST time. Retranslating from the
    // pop-up's own language buttons moves it again, and switching provider
    // rebuilds the engine list underneath it. Re-copy whenever the original
    // changes; the timer coalesces a rebuild's burst of model signals into one
    // pass, after the main window has finished repopulating.
    m_mirroredCombos = {
        {ui->engineComboBox, parent->getEngineComboBox()},
        {ui->sourceVoiceComboBox, parent->sourceVoiceComboBox()},
        {ui->translationVoiceComboBox, parent->translationVoiceComboBox()},
        {ui->sourceSpeakerComboBox, parent->sourceSpeakerComboBox()},
        {ui->translationSpeakerComboBox, parent->translationSpeakerComboBox()},
    };
    m_mirroredVisibility = {
        {ui->sourcePlayPauseButton, parent->sourcePlayPauseButton()},
        {ui->sourceStopButton, parent->sourceStopButton()},
        {ui->translationPlayPauseButton, parent->translationPlayPauseButton()},
        {ui->translationStopButton, parent->translationStopButton()},
    };

    m_syncTimer = new QTimer(this);
    m_syncTimer->setSingleShot(true);
    m_syncTimer->setInterval(0);
    connect(m_syncTimer, &QTimer::timeout, this, &PopupWindow::syncFromMainWindow);

    syncFromMainWindow();

    for (const auto &[popupCombo, mainWindowCombo] : std::as_const(m_mirroredCombos)) {
        if (mainWindowCombo == nullptr) {
            continue;
        }
        connect(popupCombo, qOverload<int>(&QComboBox::currentIndexChanged), mainWindowCombo, &QComboBox::setCurrentIndex);
        connect(mainWindowCombo, qOverload<int>(&QComboBox::currentIndexChanged), m_syncTimer, qOverload<>(&QTimer::start));
        // A rebuild clears and refills, which the combo's own signals do not
        // report - only its model's do.
        const QAbstractItemModel *model = mainWindowCombo->model();
        connect(model, &QAbstractItemModel::modelReset, m_syncTimer, qOverload<>(&QTimer::start));
        connect(model, &QAbstractItemModel::rowsInserted, m_syncTimer, qOverload<>(&QTimer::start));
        connect(model, &QAbstractItemModel::rowsRemoved, m_syncTimer, qOverload<>(&QTimer::start));
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

    connect(ui->sourcePlayPauseButton, &QToolButton::clicked, parent, &MainWindow::sourcePlayPauseClicked);
    connect(ui->sourceStopButton, &QToolButton::clicked, parent, &MainWindow::sourceStopClicked);
    connect(ui->translationPlayPauseButton, &QToolButton::clicked, parent, &MainWindow::translationPlayPauseClicked);
    connect(ui->translationStopButton, &QToolButton::clicked, parent, &MainWindow::translationStopClicked);

    // Language buttons
    if (parent->sourceLanguageButtons() != nullptr) {
        connectLanguageButtons(ui->sourceLanguagesWidget, parent->sourceLanguageButtons());
    }
    if (parent->translationLanguageButtons() != nullptr) {
        connectLanguageButtons(ui->translationLanguagesWidget, parent->translationLanguageButtons());
    }

    // Buttons
    if (parent->translateButton() != nullptr) {
        connect(ui->translateButton, &QToolButton::clicked, parent->translateButton(), &QToolButton::click);
    }
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

    // Status strip: mirrors MainWindow's model, and only appears while
    // something is running so the resting pop-up is unchanged.
    ui->statusStrip->setModel(parent->moduleStatus());
    ui->statusStrip->setHideWhenIdle(true);

    loadSettings();
}

PopupWindow::~PopupWindow()
{
    if (m_textChangedConnection != nullptr) {
        disconnect(m_textChangedConnection);
    }
    delete ui;
}

// Copy what the main window's controls currently hold. The playback buttons
// carry no state of their own beyond their icon, so their visibility - the
// provider's decision, taken in updateProviderUI() - is all there is to
// mirror. isVisibleTo() rather than isVisible() because a pop-up exists
// precisely when the main window is hidden, where every child answers false.
void PopupWindow::syncFromMainWindow()
{
    if (m_parent.isNull()) {
        return;
    }

    for (const auto &[popupCombo, mainWindowCombo] : std::as_const(m_mirroredCombos)) {
        if (mainWindowCombo == nullptr) {
            continue;
        }
        // Blocked: this runs again on every later change, and by then the
        // pop-up's own currentIndexChanged is wired back into this combo.
        const QSignalBlocker blocker(popupCombo);
        popupCombo->clear();
        for (int i = 0; i < mainWindowCombo->count(); ++i) {
            popupCombo->addItem(mainWindowCombo->itemIcon(i), mainWindowCombo->itemText(i), mainWindowCombo->itemData(i));
        }
        popupCombo->setCurrentIndex(mainWindowCombo->currentIndex());
        popupCombo->setVisible(mainWindowCombo->isVisibleTo(m_parent));
    }

    for (const auto &[popupWidget, mainWindowWidget] : std::as_const(m_mirroredVisibility)) {
        if (mainWindowWidget == nullptr) {
            continue;
        }
        popupWidget->setVisible(mainWindowWidget->isVisibleTo(m_parent));
        if (auto *popupButton = qobject_cast<QToolButton *>(popupWidget)) {
            popupButton->setIcon(qobject_cast<const QToolButton *>(mainWindowWidget)->icon());
        }
    }
}

void PopupWindow::loadSettings()
{
    const AppSettings settings;
    setWindowOpacity(settings.popupOpacity());
    resize(settings.popupWidth(), settings.popupHeight());
    ui->statusStrip->setShown(settings.isShowStatusBar());

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
    // screenAt() returns nullptr when the point isn't within any screen's
    // geometry (stale/unreliable cursor tracking, a monitor hot-unplug, or -
    // as this project's own test suite found - the popup's own parent window
    // never having been shown/exposed at all, which is the normal state when
    // PopupWindow mode is triggered from the tray/a hotkey with the main
    // window hidden). Fall back to the screen this window itself is on, or
    // the primary screen, rather than crashing.
    const QScreen *screen = QGuiApplication::screenAt(position);
    if (screen == nullptr)
        screen = QWidget::screen();
    if (screen == nullptr)
        screen = QGuiApplication::primaryScreen();
    const QSize availableSize = screen != nullptr ? screen->availableSize() : size();

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
    case QEvent::WindowActivate:
        qDebug() << "PopupWindow::event - WindowActivate";
        emit windowReady();
        break;
    case QEvent::WindowDeactivate:
        qDebug() << "PopupWindow::event - WindowDeactivate, activeModalWidget:" << QApplication::activeModalWidget();
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
