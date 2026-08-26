/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "tesseractplugin.h"

#include "tesseractocr.h"

QVector<ProviderOptionSpec> TesseractOcrPlugin::options() const
{
    return {
        makeOptionSpec(QStringLiteral("languages"),
                       QStringLiteral("OCR/Languages"),
                       QMetaType::QByteArray,
                       QByteArrayLiteral("eng"),
                       tr("Languages")),
        makeOptionSpec(QStringLiteral("languages_path"),
                       QStringLiteral("OCR/LanguagesPath"),
                       QMetaType::QByteArray,
                       QByteArray(),
                       tr("Language data directory")),
        makeOptionSpec(QStringLiteral("convert_line_breaks"),
                       QStringLiteral("OCR/ConvertLineBreaks"),
                       QMetaType::Bool,
                       true,
                       tr("Convert line breaks")),
        // A map of Tesseract variables, edited in its own table widget. No
        // single settings key holds it - it is a whole settings group - so
        // the host reads it and passes it in.
        makeDerivedOptionSpec(QStringLiteral("parameters"), QMetaType::QVariantMap, QVariantMap()),
    };
}

AOcrProvider *TesseractOcrPlugin::createProvider(QObject *parent)
{
    return new TesseractOcr(parent);
}

QStringList TesseractOcrPlugin::availableLanguages(const QString &dataPath) const
{
    return TesseractOcr::availableLanguages(dataPath);
}
