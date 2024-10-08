/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef QOPTION_H
#define QOPTION_H

#include <QJsonObject>
#include <QStringList>

/**
 * @brief Contains translation options for a single word
 *
 * Can be obtained from the QOnlineTranslator object.
 *
 * Example:
 * @code
 * QOnlineTranslator translator;
 * // Obtain translation
 *
 * QTextStream out(stdout);
 * for (auto it = translator.translationOptions().cbegin(); it != translator.translationOptions().cend(); ++it) {
 *     out << it.key() << ":" << endl; // Output the type of speech with a colon
 *     for (const auto &[word, gender, translations] : it.value()) {
 *         out << "  " << word << ": "; // Print the word
 *         out << translations; // Print translations
 *         out << endl;
 *     }
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
struct QOption {
    /**
     * @brief Word that specified for translation options.
     */
    QString word;

    /**
     * @brief Gender of the word.
     */
    QString gender;

    /**
     * @brief Associated translations for the word.
     */
    QStringList translations;

    /**
     * @brief Converts the object to JSON
     *
     * @return JSON representation
     */
    QJsonObject toJson() const;
};

#endif // QOPTION_H
