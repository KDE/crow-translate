/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "translationedit.h"

#include "contextmenu.h"

TranslationEdit::TranslationEdit(QWidget *parent)
    : QTextEdit(parent)
{
}

bool TranslationEdit::parseTranslationData(OnlineTranslator *translator)
{
    // Check for error
    if (translator->error() != OnlineTranslator::NoError) {
        clearTranslation();
        setHtml(translator->errorString());
        emit translationDataParsed(translator->errorString());
        return false;
    }

    // Store translation information
    const bool translationWasEmpty = m_translation.isEmpty();
    m_translation = translator->translation();
    m_lang = translator->translationLanguage();

    // Remove bad chars
    // Note: this hack is here, because toHtml() can't render anything with utf-8 characters
    for (int i = 0; i < m_translation.size(); ++i) {
        if (m_translation[i].category() == QChar::Symbol_Other) {
            m_translation.remove(i, 1);
            --i;
        }
    }

    // Translation
    setHtml(m_translation.toHtmlEscaped().replace(QStringLiteral("\n"), QStringLiteral("<br>")));

    // Translit
    if (!translator->translationTranslit().isEmpty()) {
        QString translit = translator->translationTranslit();
        append(QStringLiteral("<font color=\"grey\"><i>/%1/</i></font>").arg(translit.replace(QStringLiteral("\n"), QStringLiteral("/<br>/"))));
    }
    if (!translator->sourceTranslit().isEmpty()) {
        QString translit = translator->sourceTranslit();
        append(QStringLiteral("<font color=\"grey\"><i><b>(%1)</b></i></font>").arg(translit.replace(QStringLiteral("\n"), QStringLiteral("/<br>/"))));
    }

    // Transcription
    if (!translator->sourceTranscription().isEmpty())
        append(QStringLiteral("<font color=\"grey\">[%1]</font>").arg(translator->sourceTranscription()));

    append({}); // Add new line before translation options

    // Translation options
    if (!translator->translationOptions().isEmpty()) {
        append(QStringLiteral("<b>%1</b>").arg(tr("translation options:")));

        // Print words for each type of speech
        QTextBlockFormat indent;
        indent.setTextIndent(20);
        textCursor().setBlockFormat(indent);

        for (const auto &[word, translations] : translator->translationOptions()) {
            QString wordLine = word;

            // Show word meaning
            if (!translations.isEmpty())
                wordLine.append(QStringLiteral(": <font color=\"grey\"><i>%1</i></font>").arg(translations.join(QStringLiteral(", "))));

            // Add generated line to edit
            append(wordLine);
        }

        indent.setTextIndent(0);
        textCursor().setBlockFormat(indent);
        append({}); // Add an empty line in the end
    }

    // Examples
    if (!translator->examples().isEmpty()) {
        append(QStringLiteral("<b>%1</b>").arg(tr("examples:")));
        QTextBlockFormat indent;
        indent.setTextIndent(20);
        textCursor().setBlockFormat(indent);
        for (const auto &[word, example, definition, examplesSource, examplesTarget] : translator->examples()) {
            append(QStringLiteral("<i>%1</i>").arg(word));

            if (!definition.isEmpty())
                append(definition);
            if (!example.isEmpty())
                append(QStringLiteral("<font color=\"grey\"><i>%1</i></font>").arg(example));

            for (size_t i = 0; i < examplesSource.size(); ++i)
                append(QStringLiteral("%1 <font color=\"grey\"><i>%2</i></font>").arg(examplesSource[i].toHtmlEscaped(), examplesTarget[i].toHtmlEscaped()));

            append({});
        }
        indent.setTextIndent(0);
        textCursor().setBlockFormat(indent);
    }

    moveCursor(QTextCursor::Start);
    emit translationDataParsed(toHtml());
    if (translationWasEmpty)
        emit translationEmpty(false);
    return true;
}

const QString &TranslationEdit::translation() const
{
    return m_translation;
}

OnlineTranslator::Language TranslationEdit::translationLanguage()
{
    return m_lang;
}

void TranslationEdit::clearTranslation()
{
    if (!m_translation.isEmpty()) {
        m_translation.clear();
        m_lang = OnlineTranslator::NoLanguage;
        emit translationEmpty(true);
    }
    clear();
}

void TranslationEdit::contextMenuEvent(QContextMenuEvent *event)
{
    auto *contextMenu = new ContextMenu(this, event);
    contextMenu->popup();
}
