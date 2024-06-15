/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef ABSTRACTAUTOSTARTMANAGER_H
#define ABSTRACTAUTOSTARTMANAGER_H

#include <QObject>

class AbstractAutostartManager : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(AbstractAutostartManager)

public:
    static AbstractAutostartManager *createAutostartManager(QObject *parent = nullptr);

    virtual bool isAutostartEnabled() const = 0;
    virtual void setAutostartEnabled(bool enabled) = 0;

protected:
    explicit AbstractAutostartManager(QObject *parent = nullptr);

    static void showError(const QString &informativeText);
};

#endif // ABSTRACTAUTOSTARTMANAGER_H
