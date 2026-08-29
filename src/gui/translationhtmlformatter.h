/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TRANSLATIONHTMLFORMATTER_H
#define TRANSLATIONHTMLFORMATTER_H

#include <QCoreApplication>
#include <QString>

struct TranslationResult;

// Renders a TranslationResult as the HTML document the translation view
// expects, which is what MainWindow and PopupWindow hand to setHtml().
//
// This markup used to be built inside MozhiTranslationProvider, which made a
// backend responsible for one frontend's presentation and left every other
// frontend printing tags. It lives on the window's side of the line now: the
// backend reports what it found, this decides it should be grey and italic.
class TranslationHtmlFormatter
{
    Q_DECLARE_TR_FUNCTIONS(TranslationHtmlFormatter)

public:
    static QString format(const TranslationResult &result);
};

#endif // TRANSLATIONHTMLFORMATTER_H
