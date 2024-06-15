/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "ocrlanguageslistwidget.h"

OcrLanguagesListWidget::OcrLanguagesListWidget(QWidget *parent)
    : QListWidget(parent)
{
}

void OcrLanguagesListWidget::addLanguages(const QStringList &labels)
{
    for (const QString &label : labels) {
        auto *widgetItem = new QListWidgetItem(label, this);
        widgetItem->setCheckState(Qt::Unchecked);
    }
}

void OcrLanguagesListWidget::setCheckedLanguages(const QByteArray &languagesString)
{
    const QByteArrayList languages = languagesString.split('+');
    for (int i = 0; i < count(); ++i) {
        QListWidgetItem *widgetItem = item(i);
        if (languages.contains(widgetItem->text().toLocal8Bit()))
            widgetItem->setCheckState(Qt::Checked);
    }
}

QByteArray OcrLanguagesListWidget::checkedLanguagesString() const
{
    QByteArray languagesString;
    for (int i = 0; i < count(); ++i) {
        QListWidgetItem *widgetItem = item(i);
        if (widgetItem->checkState() == Qt::Checked) {
            if (!languagesString.isEmpty())
                languagesString += '+';
            languagesString += widgetItem->text().toLocal8Bit();
        }
    }
    return languagesString;
}
