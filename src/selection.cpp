/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "selection.h"

#include "popupwindow.h"

#include <QApplication>
#include <QClipboard>
#include <QDebug>
#include <QGuiApplication>
#include <QWindow>

#ifdef Q_OS_WIN
#include <QMimeData>
#include <QTimer>

#include <windows.h>

using namespace std::chrono_literals;
#endif

Selection::~Selection() = default;

Selection &Selection::instance()
{
    static Selection singletone;
    return singletone;
}

void Selection::requestSelection()
{
#if defined(Q_OS_LINUX)
    // On Wayland, clipboard access requires an active visible window
    const bool needsWindowActive = (qGuiApp->nativeInterface<QNativeInterface::QX11Application>() == nullptr);

    const bool isActive = (QGuiApplication::applicationState() == Qt::ApplicationActive);

    // Check if we have any visible QWidget windows (including popups)
    bool hasVisibleWindow = false;
    for (QWidget *widget : QApplication::topLevelWidgets()) {
        if (widget->isVisible()) {
            hasVisibleWindow = true;
            break;
        }
    }

    qDebug() << "Selection::requestSelection - needsWindowActive:" << needsWindowActive
             << "isActive:" << isActive << "hasVisibleWindow:" << hasVisibleWindow;

    if (needsWindowActive && (!isActive || !hasVisibleWindow)) {
        // Set up connection before requesting activation to avoid race condition
        if (!m_waitingForActivation) {
            m_waitingForActivation = true;
            m_activationConnection = connect(qApp, &QGuiApplication::applicationStateChanged, this, &Selection::onApplicationStateChanged);

            qDebug() << "Selection::requestSelection - emitting windowActivationNeeded";
            emit windowActivationNeeded();
        }
    } else {
        getSelection();
    }
#elif defined(Q_OS_WIN) // Send Ctrl + C to get selected text
    // Save the original clipboard
    m_originalClipboardData.reset(new QMimeData);
    const QMimeData *clipboardData = QGuiApplication::clipboard()->mimeData();
    for (const QString &format : clipboardData->formats())
        m_originalClipboardData->setData(format, clipboardData->data(format));

    // Wait until all modifiers will be unpressed (to avoid conflicts with the other shortcuts)
    while (GetAsyncKeyState(VK_LWIN) || GetAsyncKeyState(VK_RWIN) || GetAsyncKeyState(VK_SHIFT) || GetAsyncKeyState(VK_MENU) || GetAsyncKeyState(VK_CONTROL)) { };

    // Generate Ctrl + C input
    INPUT copyText[4];

    // Set the press of the "Ctrl" key
    copyText[0].ki.wVk = VK_CONTROL;
    copyText[0].ki.dwFlags = 0; // 0 for key press
    copyText[0].type = INPUT_KEYBOARD;

    // Set the press of the "C" key
    copyText[1].ki.wVk = 'C';
    copyText[1].ki.dwFlags = 0;
    copyText[1].type = INPUT_KEYBOARD;

    // Set the release of the "C" key
    copyText[2].ki.wVk = 'C';
    copyText[2].ki.dwFlags = KEYEVENTF_KEYUP;
    copyText[2].type = INPUT_KEYBOARD;

    // Set the release of the "Ctrl" key
    copyText[3].ki.wVk = VK_CONTROL;
    copyText[3].ki.dwFlags = KEYEVENTF_KEYUP;
    copyText[3].type = INPUT_KEYBOARD;

    // Send key sequence to system
    SendInput(static_cast<UINT>(std::size(copyText)), copyText, sizeof(INPUT));

    // Wait for the clipboard changes
    connect(QGuiApplication::clipboard(), &QClipboard::dataChanged, this, &Selection::getSelection);
    m_maxSelectionDelay->start();
#endif
}

#ifdef Q_OS_WIN
Selection::Selection()
    : m_maxSelectionDelay(new QTimer(this))
{
    m_maxSelectionDelay->setSingleShot(true);
    m_maxSelectionDelay->setInterval(1000ms);
#if QT_VERSION < QT_VERSION_CHECK(5, 12, 0)
    connect(m_maxSelectionWaitDelay, &QTimer::timeout, this, &Selection::saveSelection);
#else
    m_maxSelectionDelay->callOnTimeout(this, &Selection::getSelection);
#endif
}
#else
Selection::Selection() = default;
#endif

void Selection::onWindowReady()
{
    qDebug() << "Selection::onWindowReady - waitingForActivation:" << m_waitingForActivation;

    if (m_waitingForActivation) {
        m_waitingForActivation = false;
        disconnect(m_activationConnection);

        qDebug() << "Selection::onWindowReady - calling getSelection()";
        getSelection();
    }
}

void Selection::onApplicationStateChanged(Qt::ApplicationState state)
{
    qDebug() << "Selection::onApplicationStateChanged - state:" << state << "waitingForActivation:" << m_waitingForActivation;

    if (m_waitingForActivation && state == Qt::ApplicationActive) {
        m_waitingForActivation = false;
        disconnect(m_activationConnection);

        qDebug() << "Selection::onApplicationStateChanged - calling getSelection()";
        getSelection();
    }
}

void Selection::getSelection()
{
#if defined(Q_OS_LINUX)
    QClipboard *clipboard = QGuiApplication::clipboard();

    // On X11 with Selection support, use Selection clipboard (middle-click buffer)
    // On Wayland without Selection support, fall back to regular Clipboard
    if (clipboard->supportsSelection()) {
        emit requestedSelectionAvailable(clipboard->text(QClipboard::Selection));
    } else {
        emit requestedSelectionAvailable(clipboard->text(QClipboard::Clipboard));
    }
#elif defined(Q_OS_WIN)
    const QString selection = QGuiApplication::clipboard()->text();
    // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
    if (selection.isEmpty() && m_maxSelectionDelay->isActive())
        return;

    m_maxSelectionDelay->stop();
    disconnect(QGuiApplication::clipboard(), &QClipboard::dataChanged, this, &Selection::getSelection);

    // Restore the clipboard data after exit to event loop
    // clang-format off
    QMetaObject::invokeMethod(this, [this] {
        QGuiApplication::clipboard()->setMimeData(m_originalClipboardData.take());
    },  Qt::QueuedConnection);
    // clang-format on

    emit requestedSelectionAvailable(selection);
#endif
}
