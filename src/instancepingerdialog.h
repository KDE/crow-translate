/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INSTANCEPINGERDIALOG_H
#define INSTANCEPINGERDIALOG_H

#include <QProgressDialog>

class InstancePinger;

class InstancePingerDialog : public QProgressDialog
{
    Q_OBJECT

public:
    explicit InstancePingerDialog(QWidget *parent = nullptr);

    QString fastestUrl() const;

private:
    InstancePinger *m_pinger;
};

#endif // INSTANCEPINGERDIALOG_H
