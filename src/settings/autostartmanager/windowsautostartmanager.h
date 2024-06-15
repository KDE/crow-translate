/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef WINDOWSAUTOSTARTMANAGER_H
#define WINDOWSAUTOSTARTMANAGER_H

#include "abstractautostartmanager.h"

class WindowsAutostartManager : public AbstractAutostartManager
{
    Q_OBJECT
    Q_DISABLE_COPY(WindowsAutostartManager)

public:
    explicit WindowsAutostartManager(QObject *parent = nullptr);

    bool isAutostartEnabled() const override;
    void setAutostartEnabled(bool enabled) override;
};

#endif // WINDOWSAUTOSTARTMANAGER_H
