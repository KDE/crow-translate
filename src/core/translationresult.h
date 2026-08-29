/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TRANSLATIONRESULT_H
#define TRANSLATIONRESULT_H

#include <QString>
#include <QStringList>
#include <QVector>

// One dictionary sense: a word, and the ways it can be rendered.
struct TranslationOptions {
    QString word;
    QStringList translations;
};

// One dictionary entry, with whatever the backend had for it.
struct TranslationExample {
    QString word;
    QString definition;
    QString example;
    QStringList examplesSource;
    QStringList examplesTarget;
};

// What a backend found. Fields, not a rendered document.
//
// This used to be a single QString of HTML, because the one consumer at the
// time was a QTextEdit and setHtml() was the shortest route to it. That made
// every backend a renderer for one particular frontend: the markup was built
// inside MozhiTranslationProvider, the CLI printed "<br>" and
// "<font color=\"grey\">" to terminals, and speech read the tags out loud.
// Worse, it was lossy in the one direction that mattered - by the time a
// frontend saw the result, the transliteration was indistinguishable from the
// translation, so --json could not name the parts and no alternative frontend
// could lay them out differently.
//
// Backends fill this in. Frontends render it: TranslationHtmlFormatter for the
// windows, the CLI's own writer for a terminal. Adding a frontend, or a
// backend, no longer means agreeing on a markup dialect.
struct TranslationResult {
    QString translation;
    // Romanisation of the translation, and of the source.
    QString translationTranslit;
    QString sourceTranslit;
    // Phonetic transcription of the source.
    QString sourceTranscription;

    QVector<TranslationOptions> options;
    QVector<TranslationExample> examples;

    // Emptiness is about the translation alone: a result carrying only
    // transliteration is a backend that failed to translate, not a success
    // with extras.
    bool isEmpty() const
    {
        return translation.isEmpty();
    }

    void clear()
    {
        *this = TranslationResult();
    }
};

#endif // TRANSLATIONRESULT_H
