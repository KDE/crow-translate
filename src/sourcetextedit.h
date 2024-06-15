/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef SOURCETEXTEDIT_H
#define SOURCETEXTEDIT_H

#include <QPlainTextEdit>

class QTimer;

class SourceTextEdit : public QPlainTextEdit
{
    Q_OBJECT
    Q_DISABLE_COPY(SourceTextEdit)

public:
    explicit SourceTextEdit(QWidget *parent = nullptr);

    void setListenForEdits(bool listen);
    void setSimplifySource(bool enabled);
    QString toSourceText();

    // Text manipulation that preserves undo / redo history
    void replaceText(const QString &text);
    void removeText();

public slots:
    void stopEditTimer();

signals:
    void textEdited();
    void sourceEmpty(bool empty);

private slots:
    void startTimerDelay();
    void checkSourceEmptyChanged();

protected:
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    QTimer *m_textEditedTimer;
    bool m_listenForEdits = false;
    bool m_sourceEmpty = true;
    bool m_simplifySource = false;
};

#endif // SOURCETEXTEDIT_H
