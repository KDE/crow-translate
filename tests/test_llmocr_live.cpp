/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

// Live regression test for the "OCR text repeats until it runs out of
// tokens" bug (see llmocr.cpp's stop-sequence comment): drives the real
// LlmOcr class against a real, locally-running Ollama instance and a real
// vision-OCR model, instead of MockHttpServer's canned responses. The bug
// this guards against is a small model's own decoding pathology (looping
// on its output with no clean stop token) - a mocked response can echo
// whatever string a test hands it, but it cannot reproduce that failure
// mode, so only a real model can prove the fix actually holds.
//
// Any developer can run this: `ollama pull glm-ocr` (or point
// CROW_TEST_OLLAMA_MODEL at whatever vision-capable model is already
// installed) and `ollama serve`, then run the LlmOcrLiveTest ctest target
// directly. It SKIPS - never fails - when Ollama or the model isn't
// reachable, so it never blocks CI or a machine without Ollama.

#include "llm/visionmodelprobe.h"
#include "ocr/llmocr.h"

#include <QFont>
#include <QImage>
#include <QPainter>
#include <QSignalSpy>
#include <QTest>

namespace
{

QString ollamaUrl()
{
    const QByteArray fromEnv = qgetenv("CROW_TEST_OLLAMA_URL");
    return fromEnv.isEmpty() ? QStringLiteral("http://localhost:11434") : QString::fromLocal8Bit(fromEnv);
}

QString ollamaModel()
{
    const QByteArray fromEnv = qgetenv("CROW_TEST_OLLAMA_MODEL");
    return fromEnv.isEmpty() ? QStringLiteral("glm-ocr:latest") : QString::fromLocal8Bit(fromEnv);
}

// A document-like image with real, unambiguous text. The near-blank
// single-word case is exactly the pathological input that first pushed this
// model into its repeat loop during investigation of the reported bug.
QImage renderTextImage(const QString &text)
{
    QImage image(560, 200, QImage::Format_RGB32);
    image.fill(Qt::white);
    QPainter painter(&image);
    painter.setPen(Qt::black);
    QFont font = painter.font();
    font.setPointSize(16);
    painter.setFont(font);
    painter.drawText(image.rect().adjusted(16, 16, -16, -16), Qt::TextWordWrap, text);
    painter.end();
    return image;
}

} // namespace

class LlmOcrLiveTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        VisionModelProbe probe;
        QSignalSpy finishedSpy(&probe, &VisionModelProbe::finished);
        QSignalSpy failedSpy(&probe, &VisionModelProbe::failed);
        probe.probe(QStringLiteral("ollama"), ollamaUrl(), QString());

        if (!finishedSpy.wait(6000) || !failedSpy.isEmpty()) {
            QSKIP(qPrintable(QStringLiteral("Ollama not reachable at %1 - run `ollama serve` to enable this test").arg(ollamaUrl())));
        }

        const QStringList allModels = finishedSpy.constFirst().at(0).toStringList();
        const QString model = ollamaModel();
        if (!allModels.contains(model)) {
            QSKIP(qPrintable(QStringLiteral("Model \"%1\" not installed in Ollama - run `ollama pull %1` to enable this test").arg(model)));
        }
    }

    // The actual reported bug: a small local vision model (this one via
    // Ollama) finishes the real transcription, then - lacking a clean stop
    // token for the task - wraps it in a markdown fence and loops
    // re-emitting it until it exhausts its token budget, so the same text
    // lands in the source edit more than once. Reproduces it end to end
    // against the real model and asserts the fix (a stop sequence on the
    // fence marker, see llmocr.cpp) holds: one clean transcription, no
    // repeated block, no fence noise.
    void testLiveOllamaDoesNotRepeatTranscription()
    {
        const QString phrase = QStringLiteral("The quick brown fox jumps over the lazy dog. Invoice #4471 Total 128.50 EUR.");
        const QImage image = renderTextImage(phrase);

        LlmOcr ocr;
        ocr.setEndpoint(ollamaUrl(), false, QString());
        ocr.setModel(ollamaModel());
        ocr.setTimeout(120);

        QSignalSpy recognizedSpy(&ocr, &LlmOcr::recognized);
        QSignalSpy failedSpy(&ocr, &LlmOcr::failed);
        ocr.recognize(image, 96);

        QVERIFY2(recognizedSpy.wait(120000) || !failedSpy.isEmpty(), "LlmOcr neither recognized nor failed within the timeout");
        if (!failedSpy.isEmpty()) {
            QFAIL(qPrintable(QStringLiteral("LlmOcr reported failure: %1").arg(failedSpy.constFirst().at(0).toString())));
        }

        const QString recognized = recognizedSpy.constFirst().at(0).toString();
        QVERIFY2(!recognized.isEmpty(), "recognized text was empty");

        // The regression this test exists for: the model repeating its own
        // output. In practice that repeat is introduced by exactly this
        // markdown fence marker, so its presence alone is a solid signal.
        QVERIFY2(!recognized.contains(QStringLiteral("```")),
                 qPrintable(QStringLiteral("recognized text still contains fenced repeat noise: %1").arg(recognized)));

        // A distinctive substring from the source text must appear exactly
        // once - more than once is the duplicated-text bug this test guards
        // against.
        const QString needle = QStringLiteral("Invoice #4471");
        QCOMPARE(recognized.count(needle), 1);
    }
};

QTEST_MAIN(LlmOcrLiveTest)
#include "test_llmocr_live.moc"
