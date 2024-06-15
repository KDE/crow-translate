/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "qexample.h"

#include <QJsonObject>

QJsonObject QExample::toJson() const
{
    QJsonObject object{
        {"description", description},
        {"example", example},
    };

    return object;
}
