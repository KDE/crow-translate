/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef SELECTION_H
#define SELECTION_H

#include <QObject>

class QTimer;
#ifdef Q_OS_WIN
class QMimeData;
#endif

class Selection : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(Selection)

public:
    ~Selection() override;

    static Selection &instance();

signals:
    void requestedSelectionAvailable(const QString &selection);
    void windowActivationNeeded();

public slots:
    void requestSelection();
    void onWindowReady();

protected:
    Selection();

private slots:
    void getSelection();
    void onApplicationStateChanged(Qt::ApplicationState state);
#if defined(Q_OS_LINUX)
    void onActivationTimeout();
#endif

private:
    QMetaObject::Connection m_activationConnection;
    bool m_waitingForActivation = false;

#if defined(Q_OS_LINUX)
    // activateWindow() can fail to complete on Wayland (KWin's focus-stealing
    // prevention does not always grant activation) regardless of whether
    // translateSelection()/etc. was triggered by the QHotkey global shortcut
    // or a direct D-Bus call - both reach this same code path identically.
    // Without this timer, windowActivationNeeded's wait below can stall
    // forever with the selection silently dropped and no feedback.
    QTimer *m_activationTimeoutTimer;
#endif

#ifdef Q_OS_WIN
    QScopedPointer<QMimeData> m_originalClipboardData;
    QTimer *m_maxSelectionDelay;
#endif
};

#endif // SELECTION_H
