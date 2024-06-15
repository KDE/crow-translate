/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef UNIXAUTOSTARTMANAGER_H
#define UNIXAUTOSTARTMANAGER_H

#include "abstractautostartmanager.h"

class UnixAutostartManager : public AbstractAutostartManager
{
    Q_OBJECT
    Q_DISABLE_COPY(UnixAutostartManager)

public:
    explicit UnixAutostartManager(QObject *parent = nullptr);

    bool isAutostartEnabled() const override;
    void setAutostartEnabled(bool enabled) override;
};

#endif // UNIXAUTOSTARTMANAGER_H
