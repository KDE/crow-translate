/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TESSERACTOCR_H
#define TESSERACTOCR_H

#include "aocrprovider.h"
#include "cmake.h"

#include <QFuture>

#include <tesseract/baseapi.h>
#include <tesseract/ocrclass.h>

class QDir;

// Tesseract-backed OCR engine. Tesseract-specific configuration (language
// packs, parameters) lives here and on the OCR settings page; the active
// engine is selected by AppSettings::ocrEngine().
class TesseractOcr : public AOcrProvider
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", APPLICATION_ID ".TesseractOcr")
    Q_DISABLE_COPY(TesseractOcr)

public:
    explicit TesseractOcr(QObject *parent = nullptr);

    void setConvertLineBreaks(bool convert);

    QStringList availableLanguages() const;
    QByteArray languagesString() const;
    bool init(const QByteArray &languages, const QByteArray &languagesPath, const QMap<QString, QVariant> &parameters);

    QString engineName() const override;
    bool isConfigured() const override;
    void recognize(const QImage &image, int dpi) override;
    void cancel() override;

    static QStringList availableLanguages(const QString &languagesPath);

public slots:
    Q_SCRIPTABLE void applyParameters(const QMap<QString, QVariant> &parameters, bool saveSettings = false);

private:
    static QStringList parseLanguageFiles(const QDir &directory);

    QMap<QString, QVariant> m_parameters;
    QFuture<void> m_future;

    tesseract::TessBaseAPI m_tesseract;
#if TESSERACT_MAJOR_VERSION < 5
    ETEXT_DESC m_monitor;
#else
    tesseract::ETEXT_DESC m_monitor;
#endif

    bool m_convertLineBreaks = false;
};

#endif // TESSERACTOCR_H