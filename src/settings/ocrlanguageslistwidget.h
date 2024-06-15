/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LANGUAGESLISTWIDGET_H
#define LANGUAGESLISTWIDGET_H

#include <QListWidget>

class OcrLanguagesListWidget : public QListWidget
{
public:
    OcrLanguagesListWidget(QWidget *parent = nullptr);

    void addLanguages(const QStringList &labels);
    void setCheckedLanguages(const QByteArray &languagesString);
    QByteArray checkedLanguagesString() const;
};

#endif // LANGUAGESLISTWIDGET_H
