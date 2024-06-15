/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TRANSLATIONEDIT_H
#define TRANSLATIONEDIT_H

#include "qonlinetranslator.h"

#include <QTextEdit>

class TranslationEdit : public QTextEdit
{
    Q_OBJECT
    Q_DISABLE_COPY(TranslationEdit)

public:
    explicit TranslationEdit(QWidget *parent = nullptr);

    bool parseTranslationData(QOnlineTranslator *translator);
    QString translation() const;
    QOnlineTranslator::Language translationLanguage();
    void clearTranslation();

signals:
    void translationDataParsed(const QString &text);
    void translationEmpty(bool empty);

protected:
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    QString m_translation;
    QOnlineTranslator::Language m_lang = QOnlineTranslator::NoLanguage;
};

#endif // TRANSLATIONEDIT_H
