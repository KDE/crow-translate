/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

// End-to-end tests for the command-line frontend, driving the REAL built
// `crow` binary via QProcess.
//
// This has to be a subprocess test rather than an in-process one that
// constructs a Cli: the faults being pinned here are in the *process
// lifecycle* - whether an event loop is running when quit() is issued, which
// stream a message lands on, and what exit code the process finally returns.
// None of that exists for a Cli built inside a test's own QCoreApplication,
// so an in-process test would pass while the shipped binary hung.
//
// Every case runs under a wall-clock timeout; a timeout IS the failure being
// tested for, so it must never be mistaken for a slow machine. Nothing here
// touches the network: the cases either need no provider at all (--codes) or
// use the Copy provider, and a pre-seeded Instance keeps Cli::process() from
// falling into InstancePinger.

#include "mockhttpserver.h"

#include <QDir>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <QTest>

#ifndef CROW_BINARY_PATH
#error "CROW_BINARY_PATH must be defined by CMake to the built crow executable path"
#endif

namespace
{
// Long enough that a loaded machine never trips it, short enough that a real
// hang does not stall ctest: every command here is expected to finish in
// milliseconds.
constexpr int kRunTimeoutMs = 20000;

struct RunResult {
    bool finished = false; // false means it hung and had to be killed
    int exitCode = -1;
    QString out;
    QString err;
};
} // namespace

class CliTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_configDir;

    QProcessEnvironment cleanEnvironment(const QString &configDir) const
    {
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(QStringLiteral("XDG_CONFIG_HOME"), configDir.isEmpty() ? m_configDir.path() : configDir);
        // Qt built with journald support sends categorised logging to the
        // journal instead of stderr. The CLI's user-facing output must not
        // depend on that either way, so pin it off: anything this test sees
        // on stderr is there because the CLI deliberately wrote it, not
        // because a logging backend happened to be routed to the terminal.
        env.insert(QStringLiteral("QT_LOGGING_RULES"), QStringLiteral("*=false"));
        return env;
    }

    RunResult runCrow(const QStringList &args, const QString &configDir = QString()) const
    {
        RunResult result;
        QProcess process;
        process.setProcessEnvironment(cleanEnvironment(configDir));
        process.start(QStringLiteral(CROW_BINARY_PATH), args);
        if (!process.waitForStarted(5000))
            return result;

        // Spin the event loop rather than QProcess::waitForFinished(): the
        // mock HTTP server some of these cases point crow at lives in *this*
        // process, and a blocking wait would never let it accept the
        // connection the subprocess is opening.
        result.finished = QTest::qWaitFor([&process] {
            return process.state() == QProcess::NotRunning;
        },
                                          kRunTimeoutMs);
        if (!result.finished) {
            process.kill();
            process.waitForFinished(5000);
            return result;
        }
        process.waitForFinished(1000);
        result.exitCode = process.exitCode();
        result.out = QString::fromUtf8(process.readAllStandardOutput());
        result.err = QString::fromUtf8(process.readAllStandardError());
        return result;
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_configDir.isValid());
        QDir(m_configDir.path()).mkpath(QStringLiteral("crow-translate"));
        QFile config(m_configDir.filePath(QStringLiteral("crow-translate/crow-translate.conf")));
        QVERIFY(config.open(QIODevice::WriteOnly | QIODevice::Text));
        // An Instance only has to be present, never reached: no case here
        // uses a network-backed provider. Without it Cli::process() runs
        // InstancePinger against the real public instance list.
        config.write(QByteArrayLiteral(
            "[MainWindow]\n"
            "ShowPrivacyPopup=false\n"
            "\n"
            "[Translation]\n"
            "Instance=http://127.0.0.1:1\n"));
        config.close();
    }

    // --version/--help are handled by QCommandLineParser, which exits the
    // process itself. They terminate even with the quit()-before-exec() bug
    // present, so they are the control case: if these hang, something far
    // more basic is wrong than what the rest of this test is pinning.
    void testVersionTerminates()
    {
        const RunResult result = runCrow({QStringLiteral("--version")});
        QVERIFY2(result.finished, "crow --version did not terminate");
        QCOMPARE(result.exitCode, 0);
        QVERIFY(result.out.contains(QStringLiteral("crow-translate")));
    }

    // OCR from the command line at all: before the session owned the
    // recognition engines there was no way to reach them without a window,
    // and --image did not exist.
    //
    // A blank image is deliberate. What is being pinned is that the OCR path
    // *terminates* - it is a chain of asynchronous signals ending in a quit,
    // which is exactly the shape of every hang this suite exists to catch -
    // not that Tesseract reads any particular picture correctly. Which of the
    // two failure messages comes back depends on whether this machine has
    // language data installed, so both are accepted; the exit code and the
    // termination are not negotiable.
    void testImageWithNothingToReadTerminates()
    {
        QImage blank(200, 80, QImage::Format_RGB32);
        blank.fill(Qt::white);
        const QString path = m_configDir.filePath(QStringLiteral("blank.png"));
        QVERIFY2(blank.save(path, "PNG"), "could not write the test image");

        const RunResult result = runCrow({QStringLiteral("--image"), path, QStringLiteral("--tp"), QStringLiteral("copy")});
        QVERIFY2(result.finished, "crow --image hung instead of exiting");
        QVERIFY2(result.exitCode != 0, "an image with no text in it exited successfully");
        QVERIFY2(result.err.contains(QStringLiteral("recognized"), Qt::CaseInsensitive)
                     || result.err.contains(QStringLiteral("not configured"), Qt::CaseInsensitive),
                 qPrintable(QStringLiteral("unexpected stderr: %1").arg(result.err)));
    }

    // Regression: printLangCodes() returns straight out of Cli::process(),
    // which used to run before QCoreApplication::exec() had started. The
    // quit() it issued was discarded and the process sat forever, having
    // already done its work - and having printed nothing, because the
    // QTextStream it wrote to is only flushed when Cli is destroyed.
    void testCodesTerminatesAndPrints()
    {
        const RunResult result = runCrow({QStringLiteral("--codes")});
        QVERIFY2(result.finished, "crow --codes hung instead of exiting");
        QCOMPARE(result.exitCode, 0);
        QVERIFY2(!result.out.trimmed().isEmpty(), "crow --codes printed nothing");
    }

    // Regression, same root cause as above but reached through a provider:
    // Copy answers synchronously, so the whole translation completed inside
    // Cli::process() before there was an event loop to quit.
    void testSynchronousProviderTerminates()
    {
        const RunResult result = runCrow({QStringLiteral("--tp"),
                                          QStringLiteral("copy"),
                                          QStringLiteral("-s"),
                                          QStringLiteral("en"),
                                          QStringLiteral("-t"),
                                          QStringLiteral("en"),
                                          QStringLiteral("round trip")});
        QVERIFY2(result.finished, "crow --tp copy hung after completing its translation");
        QCOMPARE(result.exitCode, 0);
        QVERIFY(result.out.contains(QStringLiteral("round trip")));
    }

    // Regression: every one of these went through qCritical(). On a Qt built
    // with journald support that is routed to the journal, so the user got a
    // bare usage message with no indication of what was actually wrong. The
    // reason has to be on stderr, and it has to be on stderr rather than
    // stdout so that redirecting the translation does not capture it.
    void testErrorReasonReachesStderr_data()
    {
        QTest::addColumn<QStringList>("args");
        QTest::addColumn<QString>("expected");

        QTest::newRow("unknown source language")
            << QStringList{QStringLiteral("-s"), QStringLiteral("zzz"), QStringLiteral("-t"), QStringLiteral("de"), QStringLiteral("hi")}
            << QStringLiteral("zzz");
        QTest::newRow("unknown translation language")
            << QStringList{QStringLiteral("-s"), QStringLiteral("en"), QStringLiteral("-t"), QStringLiteral("zzz"), QStringLiteral("hi")}
            << QStringLiteral("zzz");
        QTest::newRow("unknown translation provider")
            << QStringList{QStringLiteral("--tp"), QStringLiteral("nonesuch"), QStringLiteral("hi")}
            << QStringLiteral("nonesuch");
        QTest::newRow("incompatible options")
            << QStringList{QStringLiteral("-j"), QStringLiteral("-b"), QStringLiteral("hi")}
            << QStringLiteral("brief");
        QTest::newRow("no text to translate")
            << QStringList{QStringLiteral("-s"), QStringLiteral("en"), QStringLiteral("-t"), QStringLiteral("de")}
            << QStringLiteral("no text");
        // --image supplies the source text, so anything else that also
        // supplies it is a contradiction rather than an addition.
        QTest::newRow("image with text arguments")
            << QStringList{QStringLiteral("--image"), QStringLiteral("/nonexistent.png"), QStringLiteral("hi")}
            << QStringLiteral("cannot be combined");
        QTest::newRow("image with file")
            << QStringList{QStringLiteral("--image"), QStringLiteral("/nonexistent.png"), QStringLiteral("-f")}
            << QStringLiteral("image");
        QTest::newRow("image with stdin")
            << QStringList{QStringLiteral("--image"), QStringLiteral("/nonexistent.png"), QStringLiteral("-i")}
            << QStringLiteral("image");
        QTest::newRow("unreadable image")
            << QStringList{QStringLiteral("--image"), QStringLiteral("/nonexistent.png")}
            << QStringLiteral("unable to read");
    }

    void testErrorReasonReachesStderr()
    {
        QFETCH(QStringList, args);
        QFETCH(QString, expected);

        const RunResult result = runCrow(args);
        QVERIFY2(result.finished, "crow hung on an invalid-argument path");
        QVERIFY2(result.exitCode != 0, "an invalid invocation exited successfully");
        QVERIFY2(result.err.contains(expected, Qt::CaseInsensitive),
                 qPrintable(QStringLiteral("stderr never mentioned '%1'. stderr was:\n%2").arg(expected, result.err)));
        QVERIFY2(result.out.isEmpty(),
                 qPrintable(QStringLiteral("diagnostics leaked onto stdout:\n%1").arg(result.out)));
    }

    // Regression: a provider that cannot speak emits no state change at all,
    // and speakText() sat waiting for one. --tts none is the reachable case
    // that needs no misconfiguration whatsoever - NoopTTSProvider reports
    // state() == Ready and its say() does nothing - so asking to speak with
    // it hung the process after printing the translation. A user whose
    // settings default to no TTS provider hit the same thing with a bare -r.
    //
    // Copy keeps this off the network: it answers synchronously, so the run
    // reaches the speech step with no instance involved.
    void testUnusableTtsReportsInsteadOfHanging()
    {
        const RunResult result = runCrow({QStringLiteral("--tp"),
                                          QStringLiteral("copy"),
                                          QStringLiteral("-s"),
                                          QStringLiteral("en"),
                                          QStringLiteral("-t"),
                                          QStringLiteral("en"),
                                          QStringLiteral("--tts"),
                                          QStringLiteral("none"),
                                          QStringLiteral("-r"),
                                          QStringLiteral("spoken words")});

        QVERIFY2(result.finished, "crow hung waiting on a provider that cannot speak");
        QVERIFY2(result.exitCode != 0, "speech was requested and never happened, but the run reported success");
        QVERIFY2(result.err.contains(QStringLiteral("text-to-speech is unavailable")),
                 qPrintable(QStringLiteral("no reason given for the missing speech. stderr was:\n%1").arg(result.err)));
        // The translation itself must survive a speech failure.
        QVERIFY2(result.out.contains(QStringLiteral("spoken words")),
                 qPrintable(QStringLiteral("the translation was lost. stdout was:\n%1").arg(result.out)));
    }

    // Regression: every backend builds its result as HTML because MainWindow
    // renders it with setHtml(), and the CLI printed that markup verbatim.
    // A canned Mozhi response with both transliteration fields set is what
    // produces the <font color="grey"> wrappers, so it reproduces the
    // reported output exactly without depending on a live instance.
    void testProviderMarkupIsRenderedAsText()
    {
        MockHttpServer server;
        server.queueJson(200,
                         QByteArrayLiteral(R"({"engine":"duckduckgo","detected":"en",)"
                                           R"("translated-text":"Hallo Welt","source_language":"en","target_language":"de",)"
                                           R"("target_transliteration":"HALLO VELT","source_transliteration":"helo world",)"
                                           R"("word_choices":null})"));

        const RunResult result = runCrow({QStringLiteral("-u"),
                                          server.baseUrl(),
                                          QStringLiteral("-e"),
                                          QStringLiteral("duckduckgo"),
                                          QStringLiteral("-s"),
                                          QStringLiteral("en"),
                                          QStringLiteral("-t"),
                                          QStringLiteral("de"),
                                          QStringLiteral("hello world")});

        QVERIFY2(result.finished, "crow hung talking to the mock instance");
        QCOMPARE(result.exitCode, 0);
        QVERIFY2(!result.out.contains(QLatin1Char('<')),
                 qPrintable(QStringLiteral("markup reached the terminal:\n%1").arg(result.out)));
        QVERIFY(result.out.contains(QStringLiteral("Hallo Welt")));
        // The transliterations must survive the conversion, just without
        // their <font>/<i>/<b> wrappers - they are content, not decoration.
        QVERIFY2(result.out.contains(QStringLiteral("/HALLO VELT/")),
                 qPrintable(QStringLiteral("target transliteration was lost:\n%1").arg(result.out)));
        QVERIFY2(result.out.contains(QStringLiteral("(helo world)")),
                 qPrintable(QStringLiteral("source transliteration was lost:\n%1").arg(result.out)));
    }

    // The other half of the same contract: text that genuinely contained
    // angle brackets or an ampersand must come back unchanged. Copy escapes
    // on the way in (it renders through setHtml() like everything else), the
    // CLI decodes on the way out, and the round trip has to be lossless -
    // otherwise stripping markup would silently eat the user's own text.
    void testUserAngleBracketsSurviveRoundTrip()
    {
        const QString text = QStringLiteral(R"(a <b>tag</b> & "quotes" and 5 < 6)");
        const RunResult result = runCrow({QStringLiteral("--tp"),
                                          QStringLiteral("copy"),
                                          QStringLiteral("-s"),
                                          QStringLiteral("en"),
                                          QStringLiteral("-t"),
                                          QStringLiteral("en"),
                                          QStringLiteral("-b"),
                                          text});

        QVERIFY2(result.finished, "crow hung on the copy round trip");
        QCOMPARE(result.exitCode, 0);
        QCOMPARE(result.out.trimmed(), text);
    }

    // The engine name mapping used to exist twice, and the two copies had
    // drifted: the translation path rejected an unknown name while the TTS
    // path silently ignored it and fell back to the default engine. --help
    // had drifted from both - it advertised 'lingva', which was rejected
    // outright, and never mentioned deepl, mymemory or reverso, which were
    // accepted. Copy keeps this off the network; only name resolution, which
    // happens before any request, is under test.
    void testEngineNameResolution_data()
    {
        QTest::addColumn<QString>("name");
        QTest::addColumn<bool>("accepted");

        for (const char *name : {"google", "yandex", "bing", "duckduckgo", "deepl", "libretranslate", "mymemory", "reverso"})
            QTest::newRow(name) << QString::fromLatin1(name) << true;

        QTest::newRow("case insensitive") << QStringLiteral("GOOGLE") << true;
        // Mozhi dropped Lingva; it stayed in --help long after.
        QTest::newRow("lingva") << QStringLiteral("lingva") << false;
        QTest::newRow("nonsense") << QStringLiteral("bogus") << false;
    }

    void testEngineNameResolution()
    {
        QFETCH(QString, name);
        QFETCH(bool, accepted);

        const RunResult result = runCrow({QStringLiteral("-e"),
                                          name,
                                          QStringLiteral("--tp"),
                                          QStringLiteral("copy"),
                                          QStringLiteral("-s"),
                                          QStringLiteral("en"),
                                          QStringLiteral("-t"),
                                          QStringLiteral("en"),
                                          QStringLiteral("text")});
        QVERIFY(result.finished);

        if (accepted) {
            QCOMPARE(result.exitCode, 0);
        } else {
            QVERIFY2(result.exitCode != 0, qPrintable(QStringLiteral("'%1' was accepted as an engine").arg(name)));
            // The message has to name the offender - "Error: Unknown engine"
            // on its own does not tell the user which of their arguments
            // was the problem.
            QVERIFY2(result.err.contains(name),
                     qPrintable(QStringLiteral("the error never named '%1'. stderr was:\n%2").arg(name, result.err)));
        }
    }

    // --help listed TTS providers unconditionally while the parsing that
    // accepts them is #ifdef'd, so a build without Piper (or without TTS at
    // all) advertised providers it then refused. Asserted as an invariant
    // rather than against a fixed list, so it holds in every build config.
    void testAdvertisedTtsProvidersAreAccepted()
    {
        const RunResult help = runCrow({QStringLiteral("--help")});
        QVERIFY2(help.finished, "crow --help did not terminate");
        const QString helpText = help.out.simplified();
        QVERIFY(!helpText.isEmpty());

        for (const char *rawName : {"none", "mozhi", "qt", "piper"}) {
            const QString name = QString::fromLatin1(rawName);
            const bool advertised = helpText.contains(QStringLiteral("'%1'").arg(name));

            const RunResult result = runCrow({QStringLiteral("--tts"),
                                              name,
                                              QStringLiteral("--tp"),
                                              QStringLiteral("copy"),
                                              QStringLiteral("-s"),
                                              QStringLiteral("en"),
                                              QStringLiteral("-t"),
                                              QStringLiteral("en"),
                                              QStringLiteral("-r"),
                                              QStringLiteral("text")});
            QVERIFY(result.finished);

            // "accepted" means the name was recognised, not that speech
            // succeeded - none is recognised and still cannot speak.
            const bool rejectedByName = result.err.contains(QStringLiteral("Unknown TTS provider"));
            if (advertised) {
                QVERIFY2(!rejectedByName,
                         qPrintable(QStringLiteral("--help offers '%1' but the binary rejects it").arg(name)));
            } else {
                QVERIFY2(rejectedByName,
                         qPrintable(QStringLiteral("'%1' is accepted but --help never mentions it").arg(name)));
            }
        }
    }

    // --codes called itself "Display all language codes" while printing a
    // hardcoded list of 34 QLocale entries that had nothing to do with the
    // selected provider - and omitted every code QLocale cannot express,
    // which is exactly the set a user cannot guess.
    //
    // Pinned as an invariant - the list follows the provider - rather than
    // against a fixed expected list, which would just be the old bug written
    // down again. The unreachable instance URL is the point of the second
    // half: producing the list must make no request.
    void testCodesFollowsTheProvider()
    {
        const RunResult mozhi = runCrow({QStringLiteral("--tp"),
                                         QStringLiteral("mozhi"),
                                         QStringLiteral("-u"),
                                         QStringLiteral("http://127.0.0.1:1"),
                                         QStringLiteral("--codes")});
        QVERIFY2(mozhi.finished, "crow --codes hung, or waited on the unreachable instance");
        QCOMPARE(mozhi.exitCode, 0);

        const QStringList mozhiLines = mozhi.out.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        QVERIFY2(mozhiLines.size() > 100,
                 qPrintable(QStringLiteral("only %1 languages listed; the provider supports far more").arg(mozhiLines.size())));

        // Codes with no QLocale equivalent, registered by the Mozhi backend
        // itself. The old hardcoded list could not contain these.
        QVERIFY2(mozhi.out.contains(QStringLiteral("srn - ")), "Sranan Tongo missing - custom languages are not being listed");
        QVERIFY2(mozhi.out.contains(QStringLiteral("jam - ")), "Jamaican Creole missing - custom languages are not being listed");

        const RunResult copy = runCrow({QStringLiteral("--tp"), QStringLiteral("copy"), QStringLiteral("--codes")});
        QVERIFY(copy.finished);
        QCOMPARE(copy.exitCode, 0);

        const QStringList copyLines = copy.out.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        QVERIFY2(!copyLines.isEmpty(), "copy listed no languages at all");
        QVERIFY2(copyLines.size() < mozhiLines.size(),
                 "the same list came back for two providers that support different languages");
    }

    // Regression: --json printed one top-level object per target language, so
    // asking for two produced two objects back to back. That is a stream of
    // documents, not a document, and QJsonDocument::fromJson - like any
    // ordinary parser - rejects it outright.
    //
    // Copy keeps this off the network. Two identical target languages is an
    // odd thing to ask for, but it is the multi-target output path with
    // nothing else moving, which is exactly what is under test.
    void testJsonIsOneDocumentPerRun()
    {
        const RunResult result = runCrow({QStringLiteral("--tp"),
                                          QStringLiteral("copy"),
                                          QStringLiteral("-s"),
                                          QStringLiteral("en"),
                                          QStringLiteral("-t"),
                                          QStringLiteral("en+en"),
                                          QStringLiteral("-j"),
                                          QStringLiteral("good morning")});
        QVERIFY(result.finished);
        QCOMPARE(result.exitCode, 0);

        QJsonParseError parseError{};
        const QJsonDocument doc = QJsonDocument::fromJson(result.out.toUtf8(), &parseError);
        QVERIFY2(parseError.error == QJsonParseError::NoError,
                 qPrintable(QStringLiteral("output is not a JSON document (%1). stdout was:\n%2").arg(parseError.errorString(), result.out)));
        QVERIFY(doc.isObject());

        const QJsonObject root = doc.object();
        QCOMPARE(root.value(QStringLiteral("source")).toString(), QStringLiteral("good morning"));
        // Codes, not display names - this output is for programs.
        QCOMPARE(root.value(QStringLiteral("source_language")).toString(), QStringLiteral("en"));

        const QJsonArray translations = root.value(QStringLiteral("translations")).toArray();
        QCOMPARE(translations.size(), 2);
        for (const QJsonValue &entry : translations) {
            QCOMPARE(entry.toObject().value(QStringLiteral("language")).toString(), QStringLiteral("en"));
            QCOMPARE(entry.toObject().value(QStringLiteral("text")).toString(), QStringLiteral("good morning"));
        }
    }

    // Regression: -t auto took the system locale and ignored the configured
    // primary/secondary pair entirely, so the same settings and the same text
    // produced one target in the window and a different one on the command
    // line. On an English system with English source text the CLI asked for
    // English to English and handed back the input.
    //
    // Both frontends now go through TranslationLogic::preferredDestination().
    // Asserted against the request the provider actually sends, since that is
    // the only place the resolved target is observable from outside.
    void testAutoTargetUsesTheConfiguredLanguages()
    {
        QTemporaryDir configDir;
        QVERIFY(configDir.isValid());
        QDir(configDir.path()).mkpath(QStringLiteral("crow-translate"));
        QFile config(configDir.filePath(QStringLiteral("crow-translate/crow-translate.conf")));
        QVERIFY(config.open(QIODevice::WriteOnly | QIODevice::Text));
        // Primary German, source English: the primary differs from the source,
        // so the auto rule must pick German - never mind what locale the
        // machine running this test happens to be in.
        config.write(QByteArrayLiteral(
            "[MainWindow]\n"
            "ShowPrivacyPopup=false\n"
            "\n"
            "[Translation]\n"
            "Instance=http://127.0.0.1:1\n"
            "PrimaryLanguage=de\n"
            "SecondaryLanguage=en\n"));
        config.close();

        MockHttpServer server;
        server.queueJson(200,
                         QByteArrayLiteral(R"({"engine":"duckduckgo","detected":"en","translated-text":"Guten Morgen",)"
                                           R"("source_language":"en","target_language":"de"})"));

        const RunResult result = runCrow({QStringLiteral("-u"),
                                          server.baseUrl(),
                                          QStringLiteral("-e"),
                                          QStringLiteral("duckduckgo"),
                                          QStringLiteral("-s"),
                                          QStringLiteral("en"),
                                          QStringLiteral("-t"),
                                          QStringLiteral("auto"),
                                          QStringLiteral("good morning")},
                                         configDir.path());

        QVERIFY2(result.finished, "crow hung resolving the auto target");
        QVERIFY2(server.requestCount() > 0, "no request was made, so no target was resolved");
        QVERIFY2(server.requestPath(0).contains(QStringLiteral("to=de")),
                 qPrintable(QStringLiteral("auto resolved to the wrong target. request was:\n%1").arg(server.requestPath(0))));
    }
};

QTEST_GUILESS_MAIN(CliTest)

#include "test_cli.moc"
