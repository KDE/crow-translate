/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef GENERICSCREENGRABBER_H
#define GENERICSCREENGRABBER_H

#include "abstractscreengrabber.h"

class GenericScreenGrabber : public AbstractScreenGrabber
{
    Q_OBJECT
    Q_DISABLE_COPY(GenericScreenGrabber)

public:
    explicit GenericScreenGrabber(QObject *parent = nullptr);

public slots:
    void grab() override;
    void cancel() override;
};

#endif // GENERICSCREENGRABBER_H
