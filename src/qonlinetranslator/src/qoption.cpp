/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "qoption.h"

#include <QJsonArray>
#include <QJsonObject>

QJsonObject QOption::toJson() const
{
    QJsonObject object{
        {"gender", gender},
        {"translations", QJsonArray::fromStringList(translations)},
        {"word", word},
    };

    return object;
}
