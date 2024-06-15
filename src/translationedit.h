/*
 *  Copyright © 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 *  Copyright © 2022 Volk Milit <javirrdar@gmail.com>
 *
 *  This file is part of Crow Translate.
 *
 *  Crow Translate is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Crow Translate is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Crow Translate. If not, see <https://www.gnu.org/licenses/>.
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
