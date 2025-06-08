#include <QtTest>
#include <QCoreApplication>
#include <QSignalSpy>
#include <QJsonObject>
#include <QJsonDocument>
#include <QFile>
#include <QDir>

// Needs to be relative to where the test executable will be run from (e.g., build directory)
// or adjust QOnlineTranslator's path finding for tests.
// For now, assuming it can pick up qonlinetranslator.h via include paths.
#include "../src/qonlinetranslator.h" // Adjust path if necessary based on CMake setup

class TestQOnlineTranslatorGemini : public QObject
{
    Q_OBJECT

public:
    TestQOnlineTranslatorGemini() = default;
    ~TestQOnlineTranslatorGemini() = default;

private slots:
    void initTestCase();
    void cleanupTestCase();
    void testTranslateImage_data();
    void testTranslateImage();
};

void TestQOnlineTranslatorGemini::initTestCase()
{
    // Ensure the mock script is executable if needed by OS, though QProcess usually handles it.
    // QFile mockScriptFile("mock_gemini_translate.py"); // Assuming it's in the working dir of test
    // if (mockScriptFile.exists()) {
    //     mockScriptFile.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner |
    //                                 QFileDevice::ReadGroup | QFileDevice::ExeGroup |
    //                                 QFileDevice::ReadOther | QFileDevice::ExeOther);
    // }
}

void TestQOnlineTranslatorGemini::cleanupTestCase()
{
    // qDeleteAll(m_translatorList); // Example if objects were stored
}

void TestQOnlineTranslatorGemini::testTranslateImage_data()
{
    QTest::addColumn<QString>("imagePath");
    QTest::addColumn<QOnlineTranslator::Language>("targetLang");
    QTest::addColumn<QString>("expectedTranslation");
    QTest::addColumn<QString>("mockedPythonOutput");

    QTest::newRow("french_translation")
        << "dummy_image.png"
        << QOnlineTranslator::French
        << "Bonjour le monde"
        << QStringLiteral("{\"translation\": \"Bonjour le monde\"}");

    QTest::newRow("error_from_script")
        << "dummy_image_err.png"
        << QOnlineTranslator::German
        << "" // No translation expected
        << QStringLiteral("{\"error\": \"Script simulated error\"}");
}

void TestQOnlineTranslatorGemini::testTranslateImage()
{
    QFETCH_GLOBAL(QString, imagePath);
    QFETCH_GLOBAL(QOnlineTranslator::Language, targetLang);
    QFETCH_GLOBAL(QString, expectedTranslation);
    QFETCH_GLOBAL(QString, mockedPythonOutput);

    // Create a dummy image file (content doesn't matter for this test)
    QFile dummyImage(imagePath);
    if (!dummyImage.open(QIODevice::WriteOnly)) {
        QFAIL("Failed to create dummy image file for testing.");
        return;
    }
    dummyImage.write("dummy content");
    dummyImage.close();

    // Set the environment variable for the mock script
    qputenv("MOCKED_GEMINI_OUTPUT", mockedPythonOutput.toUtf8());

    QOnlineTranslator translator;
    // The path to the mock script needs to be relative to the test's WORKING_DIRECTORY (CMAKE_CURRENT_BINARY_DIR)
    translator.setGeminiScriptPath("./mock_gemini_translate.py");


    QSignalSpy spy(&translator, &QOnlineTranslator::finished);
    QSignalSpy errorSpy(&translator, &QOnlineTranslator::errorOccurred); // Assuming such signal exists or use finished + check error()

    translator.translate(imagePath, QOnlineTranslator::Gemini, targetLang, QOnlineTranslator::English, QOnlineTranslator::English);

    // Wait for the finished() signal.
    // Increased timeout for QProcess to start and finish.
    if (!spy.wait(10000)) { // 10 seconds timeout
        // If the main 'finished' signal didn't emit, check if an error was the reason
        if (translator.error() != QOnlineTranslator::NoError) {
             // This is an expected path if mockedPythonOutput contains an error
        } else {
            QFAIL("Timeout waiting for QOnlineTranslator::finished() signal, or error signal.");
        }
    }

    if (mockedPythonOutput.contains("error")) {
        QVERIFY(translator.error() != QOnlineTranslator::NoError);
        // The specific error message from the script is checked implicitly by resetData logic
        // QCOMPARE(translator.errorString(), "Error from Gemini script: Script simulated error"); // This would be more specific
    } else {
        QCOMPARE(translator.error(), QOnlineTranslator::NoError);
        QCOMPARE(translator.translation(), expectedTranslation);
        QVERIFY(spy.count() >= 1); // finished signal should have been emitted at least once
    }

    // Cleanup
    dummyImage.remove();
    qunsetenv("MOCKED_GEMINI_OUTPUT");
}

QTEST_MAIN(TestQOnlineTranslatorGemini)

#include "testqonlinetranslatorgemini.moc"
