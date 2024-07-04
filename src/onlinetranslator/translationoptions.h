/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TRANSLATIONOPTIONS_H
#define TRANSLATIONOPTIONS_H

#include <QJsonObject>
#include <QStringList>

/**
 * @brief Contains translation options for a single word
 *
 * Can be obtained from the OnlineTranslator object.
 *
 * Example:
 * @code
 * OnlineTranslator translator;
 * // Obtain translation
 *
 * QTextStream out(stdout);
 * out << "translation options:" << endl;
 * for (const auto &[word, translations] : translator.translationOptions()) {
 *     out << "  " << word << ": "; // Print the word
 *     out << translations; // Print translations
 *     out << endl;
 * }
 * @endcode
 *
 * Possible output:
 * @code
 * // verb:
 * //  sagen: say, tell, speak, mean, utter
 * //  sprechen: speak, talk, say, pronounce, militate, discourse
 * //  meinen: think, mean, believe, say, opine, fancy
 * //  heißen: mean, be called, be named, bid, tell, be titled
 * //  äußern: express, comment, speak, voice, say, utter
 * //  aussprechen: express, pronounce, say, speak, voice, enunciate
 * //  vorbringen: make, put forward, raise, say, put, bring forward
 * //  aufsagen: recite, say, speak
 *
 * // noun:
 * //  Sagen: say
 * //  Mitspracherecht: say
 * @endcode
 */
struct TranslationOptions {
    /**
     * @brief Word that specified for translation options.
     */
    QString word;

    /**
     * @brief Associated translations for the word.
     */
    QStringList translations;
};

#endif // TRANSLATIONOPTIONS_H
