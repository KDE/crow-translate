/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TRANSLATORABORTEDTRANSITION_H
#define TRANSLATORABORTEDTRANSITION_H

#include <QSignalTransition>

class QOnlineTranslator;

class TranslatorAbortedTransition : public QSignalTransition
{
public:
    explicit TranslatorAbortedTransition(QOnlineTranslator *translator, QState *sourceState = nullptr);

protected:
    bool eventTest(QEvent *event) override;

private:
    QOnlineTranslator *m_translator;
};

#endif // TRANSLATORABORTEDTRANSITION_H
