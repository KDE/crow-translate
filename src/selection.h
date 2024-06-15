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

public slots:
    void requestSelection();

protected:
    Selection();

private slots:
    void getSelection();

#ifdef Q_OS_WIN
private:
    QScopedPointer<QMimeData> m_originalClipboardData;
    QTimer *m_maxSelectionDelay;
#endif
};

#endif // SELECTION_H
