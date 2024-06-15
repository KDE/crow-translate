/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "selection.h"

#include <QClipboard>
#include <QGuiApplication>
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
    getSelection();
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

void Selection::getSelection()
{
#if defined(Q_OS_LINUX)
    emit requestedSelectionAvailable(QGuiApplication::clipboard()->text(QClipboard::Selection));
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
