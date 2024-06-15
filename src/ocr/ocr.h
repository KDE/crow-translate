/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef OCR_H
#define OCR_H

#include "cmake.h"

#include <QFuture>
#include <QObject>

#include <tesseract/baseapi.h>
#include <tesseract/ocrclass.h>

class QDir;

class Ocr : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", APPLICATION_ID ".Ocr")
    Q_DISABLE_COPY(Ocr)

public:
    explicit Ocr(QObject *parent = nullptr);

    void setConvertLineBreaks(bool convert);

    QStringList availableLanguages() const;
    QByteArray languagesString() const;
    bool init(const QByteArray &languages, const QByteArray &languagesPath, const QMap<QString, QVariant> &parameters);

    void recognize(const QPixmap &pixmap, int dpi);
    void cancel();

    static QStringList availableLanguages(const QString &languagesPath);

public slots:
    Q_SCRIPTABLE void applyParameters(const QMap<QString, QVariant> &parameters, bool saveSettings = false);

signals:
    void recognized(const QString &text);
    void canceled();

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

#endif // OCR_H
