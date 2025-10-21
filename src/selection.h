/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef SELECTION_H
#define SELECTION_H

#include <QObject>

#ifdef Q_OS_WIN
class QTimer;
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

private:
    QMetaObject::Connection m_activationConnection;
    bool m_waitingForActivation = false;

#ifdef Q_OS_WIN
    QScopedPointer<QMimeData> m_originalClipboardData;
    QTimer *m_maxSelectionDelay;
#endif
};

#endif // SELECTION_H
