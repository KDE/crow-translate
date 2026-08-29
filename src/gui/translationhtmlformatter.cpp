/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "gui/translationhtmlformatter.h"

#include "core/translationresult.h"

QString TranslationHtmlFormatter::format(const TranslationResult &result)
{
    // Deliberately byte-for-byte what MozhiTranslationProvider used to emit,
    // so the window looks exactly as it did. Escaping happens here, at the
    // point where text becomes markup, rather than in whichever backend
    // happened to produce the text.
    QString formatted = result.translation.toHtmlEscaped().replace(QStringLiteral("\n"), QStringLiteral("<br>"));

    if (!result.translationTranslit.isEmpty()) {
        QString translit = result.translationTranslit;
        formatted += QStringLiteral("<br><font color=\"grey\"><i>/%1/</i></font>").arg(translit.replace(QStringLiteral("\n"), QStringLiteral("/<br>/")));
    }

    if (!result.sourceTranslit.isEmpty()) {
        QString translit = result.sourceTranslit;
        formatted += QStringLiteral("<br><font color=\"grey\"><i><b>(%1)</b></i></font>").arg(translit.replace(QStringLiteral("\n"), QStringLiteral("/<br>/")));
    }

    if (!result.sourceTranscription.isEmpty()) {
        formatted += QStringLiteral("<br><font color=\"grey\">[%1]</font>").arg(result.sourceTranscription);
    }

    if (!result.options.isEmpty()) {
        formatted += QStringLiteral("<br><br><b>%1</b><br>").arg(tr("translation options:"));

        for (const auto &[word, translations] : result.options) {
            QString wordLine = QStringLiteral("&nbsp;&nbsp;&nbsp;&nbsp;") + word;

            if (!translations.isEmpty()) {
                wordLine += QStringLiteral(": <font color=\"grey\"><i>%1</i></font>").arg(translations.join(QStringLiteral(", ")));
            }

            formatted += wordLine + QStringLiteral("<br>");
        }
    }

    if (!result.examples.isEmpty()) {
        formatted += QStringLiteral("<br><b>%1</b><br>").arg(tr("examples:"));

        for (const auto &[word, definition, example, examplesSource, examplesTarget] : result.examples) {
            formatted += QStringLiteral("&nbsp;&nbsp;&nbsp;&nbsp;<i>%1</i><br>").arg(word);

            if (!definition.isEmpty()) {
                formatted += QStringLiteral("&nbsp;&nbsp;&nbsp;&nbsp;%1<br>").arg(definition);
            }

            if (!example.isEmpty()) {
                formatted += QStringLiteral("&nbsp;&nbsp;&nbsp;&nbsp;<font color=\"grey\"><i>%1</i></font><br>").arg(example);
            }

            for (qsizetype i = 0; i < examplesSource.size(); ++i) {
                formatted += QStringLiteral("&nbsp;&nbsp;&nbsp;&nbsp;%1 <font color=\"grey\"><i>%2</i></font><br>")
                                 .arg(examplesSource[i].toHtmlEscaped(), examplesTarget[i].toHtmlEscaped());
            }

            formatted += QStringLiteral("<br>");
        }
    }

    return formatted;
}
