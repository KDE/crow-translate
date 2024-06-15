/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "tesseractparameterstablewidget.h"

#include <QHeaderView>

TesseractParametersTableWidget::TesseractParametersTableWidget(QWidget *parent)
    : QTableWidget(parent)
{
    setColumnCount(2);
    setHorizontalHeaderLabels({tr("Property"), tr("Value")});
    horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    verticalHeader()->setVisible(false);
}

void TesseractParametersTableWidget::addParameter(const QString &key, const QVariant &value, bool edit)
{
    auto *keyWidget = new QTableWidgetItem(key);
    auto *valueWidget = new QTableWidgetItem(value.toString());
    insertRow(rowCount());
    setItem(rowCount() - 1, 0, keyWidget);
    setItem(rowCount() - 1, 1, valueWidget);
    if (edit) {
        setCurrentItem(keyWidget);
        editItem(keyWidget);
    }
}

void TesseractParametersTableWidget::setParameters(const QMap<QString, QVariant> &parameters)
{
    clearContents();
    setRowCount(0);
    for (auto it = parameters.cbegin(); it != parameters.cend(); ++it)
        addParameter(it.key(), it.value(), false);
}

QMap<QString, QVariant> TesseractParametersTableWidget::parameters() const
{
    QMap<QString, QVariant> parameters;
    for (int i = 0; i < rowCount(); ++i) {
        const QString key = item(i, 0)->text();
        const QVariant value = item(i, 1)->text();
        if (!key.isEmpty() && !value.toString().isEmpty())
            parameters.insert(key, value);
    }
    return parameters;
}

void TesseractParametersTableWidget::removeCurrent()
{
    removeRow(currentRow());
}

// Return false if any key is missing a value or vice versa. Also focus the empty cell.
bool TesseractParametersTableWidget::validateParameters()
{
    for (int i = 0; i < rowCount(); ++i) {
        QTableWidgetItem *key = item(i, 0);
        QTableWidgetItem *value = item(i, 1);
        if (key->text().isEmpty() && !value->text().isEmpty()) {
            setCurrentItem(key);
            editItem(key);
            return false;
        }
        if (!key->text().isEmpty() && value->text().isEmpty()) {
            setCurrentItem(value);
            editItem(value);
            return false;
        }
    }
    return true;
}
