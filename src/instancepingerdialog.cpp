/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "instancepingerdialog.h"

#include "cmake.h"
#include "instancepinger.h"

InstancePingerDialog::InstancePingerDialog(QWidget *parent)
    : QProgressDialog(parent)
    , m_pinger(new InstancePinger(this))
{
    setMaximum(InstancePinger::instances().size() - 1);
    setWindowTitle(APPLICATION_NAME);

    connect(m_pinger, &InstancePinger::finished, this, &InstancePingerDialog::accept);
    connect(m_pinger, &InstancePinger::processingInstance, [this](size_t instanceIndex) {
        const QString &instance = InstancePinger::instances().at(instanceIndex);
        setLabelText(tr("Detecting fastest instance.\nChecking '%1'").arg(instance));
        setValue(static_cast<int>(instanceIndex));
    });

    m_pinger->detectFastest();
}

QString InstancePingerDialog::fastestUrl() const
{
    return m_pinger->fastestInstance();
}
