/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef PORTALAUTOSTARTMANAGER_H
#define PORTALAUTOSTARTMANAGER_H

#include "abstractautostartmanager.h"

#include <QDBusInterface>

class QDBusPendingCallWatcher;

class PortalAutostartManager : public AbstractAutostartManager
{
    Q_OBJECT
    Q_DISABLE_COPY(PortalAutostartManager)

public:
    explicit PortalAutostartManager(QObject *parent = nullptr);

    bool isAutostartEnabled() const override;
    void setAutostartEnabled(bool enabled) override;

    static bool isAvailable();

signals:
    void responseParsed();

private slots:
    void parsePortalResponse(quint32, const QVariantMap &results);

private:
    QDBusInterface m_interface;
};

#endif // PORTALAUTOSTARTMANAGER_H
