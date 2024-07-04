/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TRANSLATIONEXAMPLE_H
#define TRANSLATIONEXAMPLE_H

#include <QJsonObject>

/**
 * @brief Provides storage for example usage examples for a single type of speech
 *
 * Can be obtained from the OnlineTranslator object, which contains translation example.
 *
 * Example:
 * @code
 * OnlineTranslator translator;
 * // Obtain translation
 *
 * QTextStream out(stdout);
 * out << "examples:" << endl;
 * for (const auto &[example, description] : translator.examples()) {
 *     out << "  " << description << endl;
 *     out << "  " << example << endl;
 *     out << endl;
 * }
 * @endcode
 *
 * Possible output:
 * @code
 * // noun:
 * //   an opportunity for stating one's opinion or feelings.
 * //   the voters are entitled to have their say on the treaty
 *
 * // verb:
 * //   utter words so as to convey information, an opinion, a feeling or intention, or an instruction.
 * //   "Thank you," he said
 *
 * // exclamation:
 * //   used to express surprise or to draw attention to a remark or question.
 * //   say, did you notice any blood?
 * @endcode
 */
struct TranslationExample {
    /**
     * @brief Example sentense
     */
    QString example;

    /**
     * @brief description for the example
     */
    QString description;
};

#endif // TRANSLATIONEXAMPLE_H
