/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef PROVIDERTESTFIXTURES_H
#define PROVIDERTESTFIXTURES_H

#include "instancepinger.h"
#include "language.h"
#include "mockhttpserver.h"
#include "provideroptions.h"
#include "translator/atranslationprovider.h"
#include "translator/copytranslationprovider.h"
#include "translator/localaitranslationprovider.h"
#include "translator/mozhitranslationprovider.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <memory>

// One fixture per ATranslationProvider backend, used by the generic
// contract test suite (test_translation_provider_contract.cpp) so that
// invariants of the ATranslationProvider base class - and of how
// MainWindow actually drives it (see mainwindow.cpp's
// translatorStateChanged()/on_translateButton_clicked()) - get checked
// against every backend automatically. Adding a new backend means adding
// one fixture here and one row in the test file's addBackendRows(); no new
// test bodies. Assertions that are genuinely specific to one backend's own
// implementation (LocalAI's buildPrompt QLocale bug, its detection regex,
// its timeout wiring) belong in that backend's own test file instead.
class ProviderFixture
{
public:
    virtual ~ProviderFixture() = default;
    virtual QString name() const = 0;

    virtual Language sourceLanguage() const = 0;
    virtual Language targetLanguage() const = 0;

    // A provider whose next translate(text, targetLanguage(), sourceLanguage())
    // call will reach Processed. nullptr means this backend needs live
    // network that isn't reachable right now - the caller must QSKIP, not
    // fail, since that's an environment problem, not a regression.
    virtual std::unique_ptr<ATranslationProvider> createForSuccess() = 0;

    // A provider whose next translate() call will reach Finished with an
    // error. Fills in the language pair to use (may differ from the
    // success pair above - e.g. Copy reaches its error path via a language
    // mismatch rather than a bad response).
    virtual std::unique_ptr<ATranslationProvider> createForFailure(Language &source, Language &target) = 0;

    // Whether this backend can be driven into a genuinely in-flight
    // Processing state and aborted out of it, fully locally and
    // deterministically (no live-network timing race). False just skips
    // the mid-flight-abort contract test for this backend.
    virtual bool supportsDeterministicMidFlightAbort() const
    {
        return false;
    }
    // Only called when supportsDeterministicMidFlightAbort() is true: a
    // provider whose next translate() call hangs in Processing until the
    // test calls abort() on it.
    virtual std::unique_ptr<ATranslationProvider> createForHang()
    {
        return nullptr;
    }
};

namespace
{

inline QByteArray chatCompletionJson(const QString &content)
{
    QJsonObject message;
    message.insert(QStringLiteral("role"), QStringLiteral("assistant"));
    message.insert(QStringLiteral("content"), content);
    QJsonObject choice;
    choice.insert(QStringLiteral("message"), message);
    QJsonArray choices;
    choices.append(choice);
    QJsonObject body;
    body.insert(QStringLiteral("choices"), choices);
    return QJsonDocument(body).toJson(QJsonDocument::Compact);
}

} // namespace

class CopyProviderFixture : public ProviderFixture
{
public:
    QString name() const override
    {
        return QStringLiteral("Copy");
    }
    Language sourceLanguage() const override
    {
        return Language(QLocale::system());
    }
    Language targetLanguage() const override
    {
        return Language(QLocale::system());
    }

    std::unique_ptr<ATranslationProvider> createForSuccess() override
    {
        return std::make_unique<CopyTranslationProvider>();
    }

    // Copy's only error path is a source/destination mismatch (see
    // CopyTranslationProvider::translate()) - it never makes a network
    // call, so there's no "bad response" to simulate.
    std::unique_ptr<ATranslationProvider> createForFailure(Language &source, Language &target) override
    {
        source = Language(QLocale::English);
        target = Language(QLocale::French);
        return std::make_unique<CopyTranslationProvider>();
    }

    // Copy's translate() is fully synchronous - Processing and the
    // terminal state are both emitted before translate() returns, so
    // there's no async gap to abort into.
};

class LocalAiProviderFixture : public ProviderFixture
{
public:
    QString name() const override
    {
        return QStringLiteral("LocalAI");
    }
    Language sourceLanguage() const override
    {
        return Language(QStringLiteral("es"));
    }
    Language targetLanguage() const override
    {
        return Language(QStringLiteral("en"));
    }

    std::unique_ptr<ATranslationProvider> createForSuccess() override
    {
        m_server = std::make_unique<MockHttpServer>();
        m_server->queueJson(200, chatCompletionJson(QStringLiteral("contract test output")));
        return makeProvider();
    }

    std::unique_ptr<ATranslationProvider> createForFailure(Language &source, Language &target) override
    {
        m_server = std::make_unique<MockHttpServer>();
        m_server->queueResponse(MockHttpServer::Response{.status = 500, .body = QByteArrayLiteral("internal error")});
        source = sourceLanguage();
        target = targetLanguage();
        return makeProvider();
    }

    bool supportsDeterministicMidFlightAbort() const override
    {
        return true;
    }

    std::unique_ptr<ATranslationProvider> createForHang() override
    {
        m_server = std::make_unique<MockHttpServer>();
        m_server->queueResponse(MockHttpServer::Response{.hang = true});
        return makeProvider();
    }

private:
    std::unique_ptr<ATranslationProvider> makeProvider()
    {
        auto provider = std::make_unique<LocalAiTranslationProvider>();
        ProviderOptions opts = *provider->getDefaultOptions();
        opts.setOption(QStringLiteral("url"), m_server->baseUrl());
        opts.setOption(QStringLiteral("timeout"), 300);
        provider->applyOptions(opts);
        return provider;
    }

    std::unique_ptr<MockHttpServer> m_server;
};

class MozhiProviderFixture : public ProviderFixture
{
public:
    QString name() const override
    {
        return QStringLiteral("Mozhi");
    }
    Language sourceLanguage() const override
    {
        return Language(QLocale::English);
    }
    Language targetLanguage() const override
    {
        return Language(QLocale::Spanish);
    }

    // Real network call to a third-party Mozhi instance - no local mock for
    // Mozhi's API shape exists yet. The contract test must QSKIP (not
    // fail) if it's unreachable, same tolerance already used by
    // test_translation.cpp's testMozhiTranslation().
    std::unique_ptr<ATranslationProvider> createForSuccess() override
    {
        auto provider = std::make_unique<MozhiTranslationProvider>();
        provider->setInstance(InstancePinger::instances().first());
        return provider;
    }

    std::unique_ptr<ATranslationProvider> createForFailure(Language &source, Language &target) override
    {
        // Point at a closed local port instead of a real instance - gets a
        // deterministic, fast network error without depending on a live
        // third-party service being both reachable and rejecting.
        auto provider = std::make_unique<MozhiTranslationProvider>();
        provider->setInstance(QStringLiteral("http://127.0.0.1:1"));
        source = sourceLanguage();
        target = targetLanguage();
        return provider;
    }
};

#endif // PROVIDERTESTFIXTURES_H
