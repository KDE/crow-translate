/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TESSERACTPARAMETERSTABLEWIDGT_H
#define TESSERACTPARAMETERSTABLEWIDGT_H

#include <QTableWidget>

class TesseractParametersTableWidget : public QTableWidget
{
    Q_OBJECT

public:
    explicit TesseractParametersTableWidget(QWidget *parent = nullptr);

    void setParameters(const QMap<QString, QVariant> &parameters);
    QMap<QString, QVariant> parameters() const;
    bool validateParameters();
    void removeInvalidParameters();

public slots:
    void addParameter(const QString &key = {}, const QVariant &value = {}, bool edit = true);
    void removeCurrent();
};

#endif // TESSERACTPARAMETERSTABLEWIDGT_H
