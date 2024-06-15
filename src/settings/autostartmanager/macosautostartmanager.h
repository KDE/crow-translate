/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef MACOSAUTOSTARTMANAGER_H
#define MACOSAUTOSTARTMANAGER_H

#include "abstractautostartmanager.h"

class macOSAutostartManager : public AbstractAutostartManager
{
    Q_OBJECT
    Q_DISABLE_COPY(macOSAutostartManager)

public:
    explicit macOSAutostartManager(QObject *parent = nullptr);

    bool isAutostartEnabled() const override;
    void setAutostartEnabled(bool enabled) override;
    static QString getLaunchAgentFilename();
};

#endif // MACOSAUTOSTARTMANAGER_H
