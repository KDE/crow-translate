/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "sourcetextedit.h"

#include "contextmenu.h"

#include <QTimer>

using namespace std::chrono_literals;

SourceTextEdit::SourceTextEdit(QWidget *parent)
    : QPlainTextEdit(parent)
    , m_textEditedTimer(new QTimer(this))
{
    m_textEditedTimer->setSingleShot(true);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
    m_textEditedTimer->callOnTimeout(this, &SourceTextEdit::textEdited);
#else
    connect(m_textEditedTimer, &QTimer::timeout, this, &SourceTextEdit::textEdited);
#endif
    connect(this, &SourceTextEdit::textChanged, this, &SourceTextEdit::checkSourceEmptyChanged);
}

void SourceTextEdit::setListenForEdits(bool listen)
{
    m_listenForEdits = listen;

    if (m_listenForEdits) {
        connect(this, &SourceTextEdit::textChanged, this, &SourceTextEdit::startTimerDelay);
    } else {
        m_textEditedTimer->stop();
        disconnect(this, &SourceTextEdit::textChanged, this, &SourceTextEdit::startTimerDelay);
    }
}

void SourceTextEdit::setSimplifySource(bool enabled)
{
    m_simplifySource = enabled;
}

QString SourceTextEdit::toSourceText()
{
    return m_simplifySource ? toPlainText().simplified() : toPlainText().trimmed();
}

void SourceTextEdit::replaceText(const QString &text)
{
    QTextCursor cursor = textCursor();
    cursor.select(QTextCursor::Document);
    if (text.isEmpty())
        cursor.removeSelectedText();
    else
        cursor.insertText(text);
    setTextCursor(cursor);

    // To avoid emitting textEdited signal
    m_textEditedTimer->stop();
}

void SourceTextEdit::removeText()
{
    replaceText({});
}

void SourceTextEdit::stopEditTimer()
{
    m_textEditedTimer->stop();
}

void SourceTextEdit::startTimerDelay()
{
    m_textEditedTimer->start(500ms);
}

void SourceTextEdit::checkSourceEmptyChanged()
{
    if (toPlainText().isEmpty() != m_sourceEmpty) {
        m_sourceEmpty = toPlainText().isEmpty();
        emit sourceEmpty(m_sourceEmpty);
    }
}

void SourceTextEdit::contextMenuEvent(QContextMenuEvent *event)
{
    auto *contextMenu = new ContextMenu(this, event);
    contextMenu->popup();
}
